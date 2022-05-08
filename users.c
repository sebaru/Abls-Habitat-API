/******************************************************************************************************************************/
/* users.c                      Gestion des users dans l'API HTTP WebService                                                  */
/* Projet Abls-Habitat version 4.0       Gestion d'habitat                                                09.04.2022 21:33:35 */
/* Auteur: LEFEVRE Sebastien                                                                                                  */
/******************************************************************************************************************************/
/*
 * users.c
 * This file is part of Abls-Habitat
 *
 * Copyright (C) 2010-2020 - Sebastien Lefevre
 *
 * Watchdog is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Watchdog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Watchdog; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/************************************************** Prototypes de fonctions ***************************************************/
 #include "Http.h"
 #include <crypt.h>

 extern struct GLOBAL Global;                                                                       /* Configuration de l'API */

/******************************************************************************************************************************/
/* Check_utilisateur_password: Vérifie le mot de passe fourni                                                                 */
/* Entrées: une structure util, un code confidentiel                                                                          */
/* Sortie: FALSE si erreur                                                                                                    */
/******************************************************************************************************************************/
 static gboolean Http_check_user_password( gchar *dbsalt, gchar *dbhash, gchar *pwd )
  { guchar hash[EVP_MAX_MD_SIZE*2+1], hash_bin[EVP_MAX_MD_SIZE];
    memset ( hash, 0, sizeof(hash) );
    gint md_len;
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();                                                                   /* Calcul du SHA1 */
    EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(mdctx, dbsalt, strlen(dbsalt));
    EVP_DigestUpdate(mdctx, pwd, strlen(pwd) );
    EVP_DigestFinal_ex(mdctx, hash_bin, &md_len);
    EVP_MD_CTX_free(mdctx);

    for (gint i=0; i<md_len; i++)
     { gchar chaine[3];
       g_snprintf(chaine, sizeof(chaine), "%02x", hash_bin[i] );
       g_strlcat( hash, chaine, sizeof(hash) );
     }

    return ( ! memcmp ( hash, dbhash, strlen(dbhash) ) );
  }
/******************************************************************************************************************************/
/* USER_REGISTER_request_post: Tente de connecter un utilisateur                                                              */
/* Entrées: la connexion Websocket                                                                                            */
/* Sortie : Le JWT est mis a jour                                                                                             */
/******************************************************************************************************************************/
 void USER_REGISTER_request_post ( SoupMessage *msg, JsonNode *request )
  { /*if (!Http_check_request( msg, session, 6 )) return;*/

    SoupMessageHeaders *headers;
    g_object_get ( G_OBJECT(msg), SOUP_MESSAGE_RESPONSE_HEADERS, &headers, NULL );

    if (!Json_has_member ( request, "login" ) )    { soup_message_set_status ( msg, SOUP_STATUS_BAD_REQUEST ); return; }
    if (!Json_has_member ( request, "password" ) ) { soup_message_set_status ( msg, SOUP_STATUS_BAD_REQUEST ); return; }

    gchar *login = Normaliser_chaine ( Json_get_string ( request, "login" ) );               /* Formatage correct des chaines */
    if (!login) { soup_message_set_status (msg, SOUP_STATUS_INTERNAL_SERVER_ERROR ); return; }

    JsonNode *RootNode = Json_node_create();
    DB_Read ( DOMAIN_tree_get ( "master" ), RootNode, NULL,
              "SELECT u.user_uuid,default_domain_uuid,g.access_level,username,email,enable,salt,hash "
              "FROM users AS u "
              "INNER JOIN users_grants AS g ON (u.user_uuid = g.user_uuid AND u.default_domain_uuid=g.domain_uuid) "
              "WHERE email='%s' OR username='%s' LIMIT 1", login, login );
    g_free(login);

    if (!Json_has_member ( RootNode, "user_uuid" ))
     { json_node_unref ( RootNode );
       Info_new ( __func__, LOG_WARNING, NULL, "User '%s' not found in database", Json_get_string ( request, "login" ) );
       soup_message_set_status (msg, SOUP_STATUS_UNAUTHORIZED );
       sleep(1);
       return;
     }
    if (!Json_get_bool ( RootNode, "enable" ))
     { json_node_unref ( RootNode );
       Info_new ( __func__, LOG_WARNING, NULL, "User '%s' disabled", Json_get_string ( request, "login" ) );
       soup_message_set_status (msg, SOUP_STATUS_UNAUTHORIZED );
       sleep(1);
       return;
     }

/*********************************************** Authentification du client par login mot de passe ****************************/
    if ( Http_check_user_password( Json_get_string ( RootNode, "salt" ),
                                   Json_get_string ( RootNode, "hash" ),
                                   Json_get_string ( request, "password" ) ) == FALSE )/* Comparaison MDP */
     { json_node_unref ( RootNode );
       Info_new ( __func__, LOG_WARNING, NULL, "User '%s' -> Wrong password", Json_get_string ( request, "login" ) );
       soup_message_set_status (msg, SOUP_STATUS_UNAUTHORIZED );
       sleep(1);
       return;
     }

    gchar email[128];
    g_snprintf ( email, sizeof(email), "%s", Json_get_string ( RootNode, "email" ) );

    JsonNode *response = Json_node_create();
    gchar jit_uuid[37];
    UUID_New ( jit_uuid );
    Json_node_add_string ( response, "user_uuid",    Json_get_string ( RootNode, "user_uuid" ) );
    Json_node_add_string ( response, "domain_uuid",  Json_get_string ( RootNode, "default_domain_uuid" ) );
    Json_node_add_int    ( response, "access_level", Json_get_int    ( RootNode, "access_level" ) );
    Json_node_add_string ( response, "username",     Json_get_string ( RootNode, "username" ) );
    Json_node_add_string ( response, "email",        Json_get_string ( RootNode, "email" ) );
    Json_node_add_string ( response, "jit", jit_uuid );
    Json_node_add_string ( response, "iss", "ABLS API Server" );
    Json_node_add_string ( response, "sub", "Access Token" );
    Json_node_add_string ( response, "aud", "Console or Home Browser" );
    Json_node_add_int    ( response, "exp", Json_get_int ( Global.config, "JWT_EXPIRY" ) );
    Json_node_add_int    ( response, "iat", time(NULL) );
    json_node_unref ( RootNode );

    jwt_t *token = NULL;
    if (jwt_new (&token))
     { Info_new ( __func__, LOG_ERR, NULL, "Token Creation Error (%s) for '%s", g_strerror(errno), email );
       soup_message_set_status (msg, SOUP_STATUS_UNAUTHORIZED );
       json_node_unref(response);
       return;
     }

    gchar *key = Json_get_string ( Global.config, "JWT_SECRET_KEY" );
    if (jwt_set_alg ( token, jwt_str_alg ( Json_get_string ( Global.config, "JWT_ALG" ) ), key, strlen(key) ) )
     { jwt_free (token);
       Info_new ( __func__, LOG_ERR, NULL, "Token Set Key error (%s) for '%s'", g_strerror(errno), email );
       soup_message_set_status (msg, SOUP_STATUS_UNAUTHORIZED );
       json_node_unref(response);
       return;
     }
    gchar *response_string = Json_node_to_string ( response );
    json_node_unref ( response );
    jwt_add_grants_json ( token, response_string );
    g_free(response_string);

    gchar *token_string = jwt_encode_str (token);
    if (!token_string)
     { jwt_free (token);
       Info_new ( __func__, LOG_ERR, NULL, "Token encode error (%s) for '%s'", g_strerror (errno), email );
       soup_message_set_status (msg, SOUP_STATUS_UNAUTHORIZED );
       return;
     }

    jwt_free (token);

    response = Json_node_create();                                                                        /* Last JSON Buffer */
    Json_node_add_string ( response, "token", token_string );
    jwt_free_str ( token_string );
    Http_Send_json_response ( msg, SOUP_STATUS_OK, "You are logged in", response );
    Info_new ( __func__, LOG_NOTICE, NULL, "'%s' logged in", email );
  }
/******************************************************************************************************************************/
/* USER_ADD_request_post: Repond aux requests des userss                                                                      */
/* Entrées: la connexion Websocket                                                                                            */
/* Sortie : néant                                                                                                             */
/******************************************************************************************************************************/
 void USER_ADD_request_post ( SoupMessage *msg, JsonNode *request )
  { /*if (!Http_check_request( msg, session, 6 )) return;*/

    SoupMessageHeaders *headers;
    g_object_get ( G_OBJECT(msg), SOUP_MESSAGE_RESPONSE_HEADERS, &headers, NULL );

    soup_message_headers_append ( headers, "WWW-Authenticate", "Bearer" );
    soup_message_set_status ( msg, SOUP_STATUS_UNAUTHORIZED );
  }
/******************************************************************************************************************************/
/* USER_DISCONNECT_request_post: Deconnecte un utilisateur                                                                    */
/* Entrées: la connexion Websocket                                                                                            */
/* Sortie : Le JWT est mis a jour                                                                                             */
/******************************************************************************************************************************/
 void USER_DISCONNECT_request_post ( SoupMessage *msg, JsonNode *request )
  { /*if (!Http_check_request( msg, session, 6 )) return;*/

    Http_Send_json_response ( msg, SOUP_STATUS_OK, "You are disconnected", NULL );
  }
/*----------------------------------------------------------------------------------------------------------------------------*/
