/*! Copyright 2023 Bogdan Pilyugin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *  	 \file RestApiHandler.c
 *    \version 1.0
 * 		 \date 2023-07-26
 *     \author Bogdan Pilyugin
 * 	    \brief    
 *    \details 
 *	\copyright Apache License, Version 2.0
 */

#include "SystemApplication.h"
#include <SysConfiguration.h>
#include <webguiapp.h>
#include "esp_wifi.h"

extern SYS_CONFIG SysConfig;

static void funct_addone(char *argres)
{
    int arg = atoi(argres);
    arg *= 2;
    itoa(arg, argres, 10);
}

static void funct_time(char *argres)
{
    time_t now;
    time(&now);
    snprintf(argres, MAX_DYNVAR_LENGTH, "%d", (int) now);
}

static void funct_wifiscan(char *argres)
{
    if (atoi(argres))
        WiFiScan();
}

static void funct_wifiscanres(char *argres)
{
    int arg = atoi(argres);
    wifi_ap_record_t *R = GetWiFiAPRecord(arg);
    if (!R)
        return;
    snprintf(argres, MAX_DYNVAR_LENGTH, "{\"ssid\":\"%s\",\"rssi\":%i,\"ch\":%d}", R->ssid, R->rssi,
             R->primary);
}

const rest_var_t ConfigVariables[] =
        {
                { 0, "netname", &SysConfig.NetBIOSName, VAR_STRING, 3, 31 },
                { 1, "otaurl", &SysConfig.OTAURL, VAR_STRING, 3, 128 },
                { 2, "ledenab", &SysConfig.Flags1.bIsLedsEnabled, VAR_BOOL, 0, 1 },
                { 3, "otaint", &SysConfig.OTAAutoInt, VAR_INT, 0, 65535 },
                { 4, "time", &funct_time, VAR_FUNCT, 0, 0 },
                { 5, "addone", &funct_addone, VAR_FUNCT, 0, 0 },
                { 6, "wifiscan", &funct_wifiscan, VAR_FUNCT, 0, 0 },
                { 7, "wifiscanres", &funct_wifiscanres, VAR_FUNCT, 0, 0 }

        };

esp_err_t SetConfVar(char *name, char *val, rest_var_types *tp)
{
    rest_var_t *V = NULL;
    for (int i = 0; i < sizeof(ConfigVariables) / sizeof(rest_var_t); ++i)
    {
        if (!strcmp(ConfigVariables[i].alias, name))
        {
            V = (rest_var_t*) (&ConfigVariables[i]);
            break;
        }
    }
    if (!V)
        return ESP_ERR_NOT_FOUND;
    int constr;
    *tp = V->vartype;
    switch (V->vartype)
    {
        case VAR_BOOL:
            if (!strcmp(val, "true") || !strcmp(val, "1"))
                *((bool*) V->ref) = true;
            else if (!strcmp(val, "false") || !strcmp(val, "0"))
                *((bool*) V->ref) = 0;
            else
                return ESP_ERR_INVALID_ARG;
        break;
        case VAR_INT:
            constr = atoi(val);
            if (constr < V->minlen || constr > V->maxlen)
                return ESP_ERR_INVALID_ARG;
            *((int*) V->ref) = constr;
        break;
        case VAR_STRING:
            constr = strlen(val);
            if (constr < V->minlen || constr > V->maxlen)
                return ESP_ERR_INVALID_ARG;
            strcpy(V->ref, val);
        break;
        case VAR_FUNCT:
            ((void (*)(char*)) (V->ref))(val);
        break;

    }
    return ESP_OK;
}

esp_err_t GetConfVar(char *name, char *val, rest_var_types *tp)
{
    rest_var_t *V = NULL;
    for (int i = 0; i < sizeof(ConfigVariables) / sizeof(rest_var_t); ++i)
    {
        if (!strcmp(ConfigVariables[i].alias, name))
        {
            V = (rest_var_t*) (&ConfigVariables[i]);
            break;
        }
    }
    if (!V)
        return ESP_ERR_NOT_FOUND;
    *tp = V->vartype;
    switch (V->vartype)
    {
        case VAR_BOOL:
            strcpy(val, *((bool*) V->ref) ? "1" : "0");
        break;
        case VAR_INT:
            itoa(*((int*) V->ref), val, 10);
        break;
        case VAR_STRING:
            strcpy(val, (char*) V->ref);
        break;

        case VAR_FUNCT:
            ((void (*)(char*)) (V->ref))(val);
        break;

    }

    //val = V->ref;
    return ESP_OK;
}

