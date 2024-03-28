include(${CMAKE_CURRENT_LIST_DIR}/extlibs/jReadWrite/include.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/extlibs/libespfs/cmake/include.cmake)

if(CONFIG_WEBGUIAPP_LORAWAN_ENABLE)
 set(lora_SRCS "src/LoRaWAN.c")
endif()

if(CONFIG_WEBGUIAPP_GPRS_ENABLE)
 set(gprs_SRCS  "src/GSMTransport.c")
endif()

idf_component_register( 
    SRCS "src/SysConfiguration.c"
    	 "src/romfs.c"
    	 "src/spifs.c"
    	 "src/HTTPServer.c"
    	 "src/FileServer.c"
    	 "src/HTTPAPISystem.c"
    	 "src/CommandProcSys.c"
    	 "src/Helpers.c"
    	 "src/NetTransport.c"
    	 "src/WiFiTransport.c"
    	 "src/ETHTransport.c"
    	 "src/SNTP.c"
    	 "src/MQTT.c"
    	 "src/CronTimers.c"
    	 "src/SerialPort.c"
    	  src/FileBlockHandler.c
    	  src/OTA.c
    	  src/RestApiHandler.c
    	  src/SysComm.c
    	  src/SysErr.c
    	  ${lora_SRCS}
    	  ${gprs_SRCS}
    	  ${jreadwrite_SRCS}
    	  ${libespfs_SRCS}
    	    
    INCLUDE_DIRS  "."
    			  "include"
    			  "src"
    			  ${jreadwrite_INCLUDE_DIRS}
    			  ${libespfs_INCLUDE_DIRS}
    			  
    PRIV_INCLUDE_DIRS ${libespfs_PRIV_INCLUDE_DIRS}			  
    			   
    REQUIRES nvs_flash 
    		 spiffs
    		 esp_http_server
    		 mbedtls
    		 lwip
    		 mqtt
    		 esp_https_ota
    		 app_update
			 esp_wifi
			 esp_http_server
			 esp_eth
			 esp_cron
			 esp_modem
			 ttn-esp32
			
			 
	PRIV_REQUIRES ${libespfs_PRIV_REQUIRES}
		
	EMBED_FILES "upload_script.html"
	EMBED_TXTFILES ca_cert.pem
) 