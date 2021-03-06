

#pragma once

#include <nstd/String.h>
#include <nstd/HashMap.h>
#include <nstd/Buffer.h>

class HttpRequest
{
public:
  HttpRequest();
  ~HttpRequest();

  const String& getErrorString() {return error;}

  bool_t get(const String& url, Buffer& data, bool checkCertificate = true);
  bool_t post(const String& url, const HashMap<String, String>& formData, Buffer& data);

private:
  void* curl;
  String error;
};
