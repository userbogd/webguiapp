include(${CMAKE_CURRENT_LIST_DIR}/extlibs/jReadWrite/include.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/extlibs/libespfs/cmake/include.cmake)

if(CONFIG_WEBGUIAPP_LORAWAN_ENABLE)
    set(lora_SRCS "src/LoRaWAN.c")
    set(lora_REQUIRES ttn-esp32)
endif()

idf_component_register(
	
	SRCS ${jreadwrite_SRCS}
    INCLUDE_DIRS ${jreadwrite_INCLUDE_DIRS}
    
    SRCS ${lora_SRCS}
    
    SRCS ${libespfs_SRCS}
    INCLUDE_DIRS ${libespfs_INCLUDE_DIRS}
    PRIV_INCLUDE_DIRS ${libespfs_PRIV_INCLUDE_DIRS}
    PRIV_REQUIRES ${libespfs_PRIV_REQUIRES}
    
    SRCS "src/SystemConfiguration.c"
    	 "src/romfs.c"
    	 "src/spifs.c"
    	 "src/HTTPServer.c"
    	 "src/FileServer.c"
    	 "src/HTTPPrintSystem.c"
    	 "src/HTTPPostSystem.c"
    	 "src/Helpers.c"
    	 "src/NetTransport.c"
    	 "src/WiFiTransport.c"
    	 "src/GSMTransport.c"
    	 "src/ETHTransport.c"
    	 "src/SNTP.c"
    	 "src/MQTT.c"
    	 "src/MQTTSysHandler.c"
    	 "src/OTA.c"
   	 
    	    
    INCLUDE_DIRS  "."
    			  "include"
    			  "src"
    			  "extlibs/jReadWrite"
    
    EMBED_FILES "upload_script.html"			  
    			   
    REQUIRES nvs_flash 
    		 spiffs
    		 esp_http_server
    		 mbedtls
    		 lwip
    		 mqtt
    		 esp_modem
    		 esp_https_ota
    		 app_update
	REQUIRES ${lora_REQUIRES}

) 