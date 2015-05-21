#include "ByteStreamHandlerInfo.h"

ByteStreamHandlerInfo::ByteStreamHandlerInfo(const std::wstring& fileExtension, const std::wstring& mimeType) : _fileExtension(fileExtension), _mimeType(mimeType)
{
}

ByteStreamHandlerInfo::~ByteStreamHandlerInfo(void)
{
}

std::wstring ByteStreamHandlerInfo::GetMimeType() const
{
  return _mimeType;
}

std::wstring ByteStreamHandlerInfo::GetFileExtension() const
{
  return _fileExtension;
}

bool ByteStreamHandlerInfo::operator==(const ByteStreamHandlerInfo &other) const
{
  return (other._fileExtension == this->_fileExtension && other._mimeType == this->_mimeType);
}