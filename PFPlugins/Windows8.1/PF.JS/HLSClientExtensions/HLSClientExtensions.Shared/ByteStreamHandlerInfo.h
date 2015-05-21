#pragma once
#include <string>

// Simple class to hold data for registering the byte stream handler
class ByteStreamHandlerInfo
{
public:
  ByteStreamHandlerInfo(const std::wstring& fileExtension, const std::wstring& mimeType);
  ~ByteStreamHandlerInfo(void);

  std::wstring GetMimeType() const;
  std::wstring GetFileExtension() const;

  bool operator==(const ByteStreamHandlerInfo &other) const;

private:
  std::wstring _fileExtension;
  std::wstring _mimeType;
};

