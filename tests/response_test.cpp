#include "HTTPResponse.h"
#include <gtest/gtest.h>
TEST(HTTPResponseTest, SerializeStatusAndBody) {
  HTTPResponse respo;
  respo.statusCode = 200;
  respo.reasonPhrase = "OK";
  respo.body = "Hello";
  std::string raw = respo.serialize();
  EXPECT_EQ(raw.rfind("HTTP/1.1 200 OK\r\n", 0), 0u);
  EXPECT_NE(raw.find("\r\n\r\nHello"), std::string::npos);
  EXPECT_EQ(raw.substr(raw.size() - 5), "Hello");
}

TEST(HTTPResponseTest, SerializeContentLengthAndConnection) {
  HTTPResponse re;
  re.body = "Hello";
  re.headers["Content-Length"] = "5";
  re.headers["Connection"] = "keep-alive";

  std::string raw=re.serialize();

  EXPECT_NE(raw.find("Content-Length: 5\r\n"), std::string::npos);
  EXPECT_NE(raw.find("Connection: keep-alive\r\n"), std::string::npos);
  EXPECT_EQ(raw.find("\r\n\r\n"), raw.rfind("\r\n\r\n"));
}

TEST(HTTPResponseTest, SerializeEmptyBody) {
  HTTPResponse res;
  res.body.clear();

  std::string raw = res.serialize();
  EXPECT_EQ(raw.substr(raw.size() - 4), "\r\n\r\n");
}
TEST(HTTPResponseTest, HeaderBlockExcludesBody) {
  HTTPResponse res;
  res.body = "Hello";
  res.headers["Transfer-Encoding"] = "chunked";
  std::string raw = res.serializeHeaders();
  EXPECT_NE(raw.find("Transfer-Encoding: chunked\r\n"), std::string::npos);
  EXPECT_EQ(raw.find("Content-Length"), std::string::npos);
  EXPECT_EQ(raw.find("Hello"), std::string::npos);
  EXPECT_EQ(raw.substr(raw.size() - 4), "\r\n\r\n");
}

TEST(HTTPResponseTest, FormatsOneChunkAndTerminator) {
    EXPECT_EQ(HTTPResponse::formatChunk("Hello"), "5\r\nHello\r\n");
    EXPECT_EQ(HTTPResponse::formatChunk(" World"), "6\r\n World\r\n");
    EXPECT_EQ(HTTPResponse::chunkTerminator(), "0\r\n\r\n");
}