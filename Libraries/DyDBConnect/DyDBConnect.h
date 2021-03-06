#ifndef __DY_DBCONNECT_H__
#define __DY_DBCONNECT_H__

#include <Arduino.h>

class DyDBConnect{
private:
	String baseUrl;
public:
	DyDBConnect(String url_);
	DyDBConnect() = delete;

	void get(String endpoint, float val);

};
#endif
