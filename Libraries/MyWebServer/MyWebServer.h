#ifndef __WEB_SERVER_H___
#define __WEB_SERVER_H___

#include <WebServer.h>
class MyWebServer{
private:
	void handleRoot();
	void handleNotFound();
public:
	//MyWebServer(){};
	int getSwitchVal();
	void handleClient();
	void begin();
	void API_setup();
};

#endif
