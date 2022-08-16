idf_component_register(
    SRCS "src/SystemConfiguration.c"
    	 "src/romfs.c"
    	 "src/HTTPServer.c"
    	 "src/HTTPPrintSystem.c"
    	 "src/HTTPPostSystem.c"
    	 "src/Helpers.c"
    	 "src/NetTransport.c"
    	 "src/WiFiTransport.c"
    	 "src/GSMTransport.c"
    			   
    INCLUDE_DIRS  "." "include"  "src"   
    REQUIRES nvs_flash 
    		 libespfs
    		 esp_http_server
    		 mbedtls
    		 lwip
    		 mqtt
    		 esp_modem
    		 
) 