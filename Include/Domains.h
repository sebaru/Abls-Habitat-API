/******************************************************************************************************************************/
/* Include/Domains.h        Déclaration structure internes des domaines                                                       */
/* Projet Abls-Habitat version 4.x       Gestion d'habitat                                                19.02.2022 20:58:23 */
/* Auteur: LEFEVRE Sebastien                                                                                                  */
/******************************************************************************************************************************/
/*
 * Domains.h
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


#ifndef _DOMAIN_H_
 #define _DOMAIN_H_

 struct DOMAIN                                                                                          /* Zone des domaines */
  { JsonNode *config;
    pthread_mutex_t synchro;
    MYSQL *mysql;
    MYSQL *arch_mysql;
    gchar mysql_last_error[256];
    GTree *Visuels;
    gint Nbr_visuels;
    GSList *ws_agents;
  };

/*************************************************** Définitions des prototypes ***********************************************/
 extern struct DOMAIN *DOMAIN_tree_get ( gchar *domain_uuid );
 extern void DOMAIN_Load ( JsonArray *array, guint index_, JsonNode *domaine_config, gpointer user_data );
 extern void DOMAIN_Load_all ( void );
 extern void DOMAIN_Unload_all( void );
 extern void DOMAIN_request ( SoupServer *server, SoupMessage *msg, const char *path, GHashTable *query,
                              SoupClientContext *client, gpointer user_data );
 #endif
/*----------------------------------------------------------------------------------------------------------------------------*/
