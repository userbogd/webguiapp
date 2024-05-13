 /* Copyright 2024 Bogdan Pilyugin
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
 *   File name: AstroRelay.c
 *     Project: WebguiappTemplate
 *  Created on: 2024-05-13
 *      Author: bogd
 * Description:	
 */

#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "CronTimers.h"
#include "esp_log.h"

#define C (3.14159265/180.0)
#define B (180.0/3.14159265)
#define zenith 90.8335f

#define LAT 54.73
#define LON 20.51
#define SUN_ANG 0

static uint16_t srTime = 0;
static uint16_t ssTime = 0;

static double Lat, Lon, Ang;



static double GetSunEvent(uint8_t event, uint16_t day);

uint16_t NumberDayFromUnix(uint32_t t)
{
  time_t clock;
  struct tm * tp;
  clock = t;
  tp = gmtime(&clock);
  return ((uint16_t) tp->tm_yday) + 1;
}

void SetSunConditions(double lat, double lon, double ang)
{
    Lat = lat;
    Lon = lon;
    Ang = ang;
}

void SetSunTimes(uint32_t t)
{
  if (1)
    {
      double tt;
      tt = GetSunEvent(0, NumberDayFromUnix(t));
      if (tt > 0)
        srTime = (uint16_t) (60.0 * tt);
      else
        srTime = 0xffff; //no valid sinrise time
      tt = GetSunEvent(1, NumberDayFromUnix(t));
      if (tt > 0)
        ssTime = (uint16_t) (60.0 * tt);
      else
        ssTime = 0xffff; //no valid sunset time
    }

     ESP_LOGI("ASTRO", "Day number %d", NumberDayFromUnix(t));
     ESP_LOGI("ASTRO", "Sanrise %dh %dm", srTime/60 + 2, srTime - (srTime/60 * 60));
     ESP_LOGI("ASTRO", "Sanset %dh %dm", ssTime/60 + 2 , ssTime - (ssTime/60 * 60));

}

uint16_t GetSunrise(void)
{
  return srTime;
}

uint16_t GetSunset(void)
{
  return ssTime;
}


static double GetSunEvent(uint8_t event, uint16_t day)
{
  double lngHour, t, M, L, RA, sinDec, cosDec, cosH, H, T, UT;
  double Lquadrant, RAquadrant;
  double zen;
  if (SUN_ANG == 0)
    zen = zenith + (double) SUN_ANG; //sunrise/set
  else
    zen = 90.0 + (double) SUN_ANG; //twilight
  lngHour = LON / 15;
  if (event == 0)
    t = day + ((6 - lngHour) / 24);
  else
    t = day + ((18 - lngHour) / 24);

  M = (0.9856 * t) - 3.289;
  L = M + (1.916 * sin(M * C)) + (0.020 * sin(2 * M * C)) + 282.634;
  if (L > 360)
    {
      L = L - 360;
    }
  else if (L < 0)
    {
      L = L + 360;
    }

  RA = B * atan(0.91764 * tan(L * C));
  if (RA > 360)
    {
      RA = RA - 360;
    }
  else if (RA < 0)
    {
      RA = RA + 360;
    }

  Lquadrant = (floor(L / 90)) * 90;
  RAquadrant = (floor(RA / 90)) * 90;
  RA = RA + (Lquadrant - RAquadrant);
  RA = RA / 15;
  sinDec = 0.39782 * sin(L * C);
  cosDec = cos(asin(sinDec));
  cosH = (cos(zen * C) - (sinDec * sin(LAT * C))) / (cosDec * cos(LAT * C));

  if (event == 0)
    { //rise
      if (cosH > 1)
        return -1;
      H = 360 - B * (acos(cosH));
    }
  else
    { //set
      if (cosH < -1)
        return -1;
      H = B * (acos(cosH));
    }

  H = H / 15;
  T = H + RA - (0.06571 * t) - 6.622;

  UT = T - lngHour;

  if (UT >= 24)
    {
      UT = UT - 24;
    }
  else if (UT < 0)
    {
      UT = UT + 24;
    }
  return UT;
}


