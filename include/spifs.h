 /*! Copyright 2022 Bogdan Pilyugin
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
 *  	 \file spifs.h
 *    \version 1.0
 * 		 \date 2022-11-13
 *     \author Bogdan Pilyugin
 * 	    \brief    
 *    \details 
 *	\copyright Apache License, Version 2.0
 */

#ifndef COMPONENTS_WEBGUIAPP_INCLUDE_SPIFS_H_
#define COMPONENTS_WEBGUIAPP_INCLUDE_SPIFS_H_

#include "esp_err.h"

esp_err_t init_spi_fs(const char *root);



#endif /* COMPONENTS_WEBGUIAPP_INCLUDE_SPIFS_H_ */
