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
 *  	 \file romfs.c
 *    \version 1.0
 * 		 \date 2022-08-14
 *     \author Bogdan Pilyugin
 * 	    \brief    
 *    \details 
 *	\copyright Apache License, Version 2.0
 */

#include "romfs.h"

extern const uint8_t espfs_bin[];
espfs_fs_t *fs;
espfs_config_t espfs_config = {
        .addr = espfs_bin,
};

void init_rom_fs(const char *root)
{
    fs = espfs_init(&espfs_config);
    assert(fs != NULL);
    /*
    esp_vfs_espfs_conf_t vfs_espfs_conf = {
            .base_path = root,
            .fs = fs,
            .max_files = 5, };
    esp_vfs_espfs_register(&vfs_espfs_conf);
     */
}
