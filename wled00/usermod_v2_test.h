#pragma once

#include "wled.h"

//This is an empty v2 usermod template. Please see the file usermod_v2_example.h in the EXAMPLE_v2 usermod folder for documentation on the functions you can use!

class UsermodASD : public Usermod {
  private:
    unsigned int userVar0;
    float var1;
    bool flag1;
    String str1;
    uint8_t var2;
    int8_t var3;
    
  public:
    UsermodASD() {
      userVar0 = 42;
      var1=2.7172;
      flag1 = false;
      str1 = "kek";
      var2 = 25;
      var3 = -1;
    }
    
    void setup() {
      
    }

    void loop() {
      
    }

    void addToConfig(JsonObject& root)
    {
      JsonObject top = root.createNestedObject("exampleUsermod");
      top["great"] = userVar0; //save this var persistently whenever settings are saved
      top["float"] = var1;
      top["flag1"] = flag1;
      top["str1"] = str1;
      top["uint8_t"] = var2;
      top["int8_t"] = var3;
    }

    bool readFromConfig(JsonObject& root)
    {
      //set defaults for variables when declaring the variable (class definition or constructor)
      JsonObject top = root["exampleUsermod"];
      if (top.isNull()) return false;

      userVar0 = top["great"] | userVar0;
      var1 = top["float"] | var1;
      flag1 = top["flag1"] | flag1; 
      str1 = top["str1"] | str1;
      var2 = top["uint8_t"] | var2;
      var3 = top["int8_t"] | var3;

      Serial.println();
      Serial.print("(int)top[\"great\"]: ");
      Serial.println((int)top["great"]);
      Serial.print("top[\"float\"]: ");
      Serial.println(top["float"].as<float>());
      Serial.print("top[\"flag1\"]: ");
      Serial.println(top["flag1"].as<bool>());
      Serial.print("top[\"str1\"]: ");
      Serial.println(top["str1"].as<String>());
      Serial.print("top[\"uint8_t\"]: ");
      Serial.println(top["uint8_t"].as<uint8_t>());
      Serial.print("top[\"int8_t\"]: ");
      Serial.println(top["int8_t"].as<int8_t>());
      Serial.print("(unsigned int)top[\"great\"]: ");
      Serial.println(top["great"].as<unsigned int>());

      // use "return !top["newestParameter"].isNull();" when updating Usermod with new features
      return true;
    }
};