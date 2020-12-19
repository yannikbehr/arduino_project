#ifndef __UTILS_H__
#define __UTILS_H__


// does not work for float: %.1f (because this function is not properly implemented on the Arduino
/*
void my_print( const char * format, ... )
{
  char buffer[200];
  va_list args;
  va_start (args, format);
  vsprintf (buffer,format, args);
  Serial.println(buffer);
  va_end (args);
}
*/

// this one can handle floats as well
void my_print( const char * format, ... )
{
  char* format_buf = new char[strlen(format)+1];
  strcpy(format_buf, format);

  va_list args;
  va_start (args, format);

  char* p0 = format_buf;
  while(true){
  	char* p1 = strchr(p0, '%');
	if (!p1){ 
		// print remaining string
        Serial.print(p0);	
		break;
	}

	// print text between format specs
    p1[0] = '\0';
	Serial.print(p0);

    // find out what type of argument we should parse
    bool keep_going = true;
    while (keep_going){
    	p1++ ; //expect at lease one more character, at least the zero termination   
    	switch (p1[0]){
          case 'i':
    	  case 'd': {
    	    int num = va_arg(args, int);
    	    //Serial.print("-i ");
    	    Serial.print(num);
            keep_going = false;
            p1++;
    	    break;
          }
    	  case 'c': {
    	    char c = (char) va_arg(args, int);
    	    //Serial.print("-c ");
    	    Serial.print(c);
            keep_going = false;
            p1++;
    	    break;
          }
    	  case 's': {
			char* s = va_arg(args, char *);
    	    //Serial.print("-s ");
    	    Serial.print(s);
            keep_going = false;
            p1++;
    	    break;
		  }
          case 'f':{
			double f = va_arg(args, double);
    	    //Serial.print("-f ");
    	    Serial.print(f);
            keep_going = false;
            p1++;
    	    break;
		  }
    	  case '0':
    	  case '1':
    	  case '2':
    	  case '3':
    	  case '4':
    	  case '5':
    	  case '6':
    	  case '7':
    	  case '8':
    	  case '9':
    	  case '.':
            // ignore all of those and move to the next letter
    	    break;
    	  case '\0':
    	    Serial.print("-- Incomplete format string --");
			keep_going = false;
    	    break ; // incomplete format string, but I don't care 
          default:
            Serial.print("-- Unknown format specifier --");
			keep_going = false;
		}
     }
 	p0 = p1; // move to next item
  }
  va_end (args);
  delete[] format_buf;
}


#endif
