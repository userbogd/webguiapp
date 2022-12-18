idf_component_register(
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
    	 "src/LoRaWANTransport.c"
    	 "src/SNTP.c"
    	 "src/MQTT.c"
    	 "src/MQTTSysHandler.c"
    	 "src/OTA.c"
    			   
    INCLUDE_DIRS  "."
    			  "include"
    			  "src"
    
    EMBED_FILES "upload_script.html"			  
    			   
    REQUIRES nvs_flash 
    		 libespfs
    		 spiffs
    		 esp_http_server
    		 mbedtls
    		 lwip
    		 mqtt
    		 esp_modem
    		 esp_https_ota
    		 app_update
    		 jReadWrite
    		 ttn-esp32
) 