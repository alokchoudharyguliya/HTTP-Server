#include "HTTPParser.h"
#include <gtest/gtest.h>
TEST(HTTPParserTest, ParsesValidGet) {
  HTTPParser parser;
  HTTPRequest req = parser.parse("GET /index.html HTTP/1.1\n\n");

  EXPECT_EQ(req.method, "GET");
  EXPECT_EQ(req.path, "/index.html");
  EXPECT_EQ(req.version, "HTTP/1.1");
  EXPECT_TRUE(req.headers.empty());
}

TEST(HTTPParserTest, ParsesMultipleHeaders) {
  HTTPParser parser;
  HTTPRequest request = parser.parse("GET / HTTP/1.1\n"
                                     "Host: localhost:8001\n"
                                     "Accept: */*\n"
                                     "\n");
  EXPECT_EQ(request.headers["Host"], "localhost:8001");
  EXPECT_EQ(request.headers["Accept"], "*/*");
}

TEST(HTTPParserTest, MissingRequestLineLeavesFieldsEmpty) {
  HTTPParser parser;
  HTTPRequest req = parser.parse("");

  EXPECT_TRUE(req.method.empty());
  EXPECT_TRUE(req.path.empty());
  EXPECT_TRUE(req.version.empty());
}

TEST(HTTPParserTest, IncompleteRequestLineLeavesVersionEmpty) {
  HTTPParser parser;
  HTTPRequest req = parser.parse("GET /\nHost: localhost\n\n");

  EXPECT_EQ(req.method, "GET");
  EXPECT_EQ(req.path, "/");
  // EXPECT_TRUE(req.version.empty());
  // EXPECT_EQ(req.headers["HOST"], " localhost");
  EXPECT_EQ(req.version, "Host:");
  EXPECT_EQ(req.headers.count("Host"), 0u);
}

TEST(HTTPParserTest, HeaderWithoutColonIsSkipped) {
  HTTPParser parser;
  HTTPRequest req = parser.parse("GET / HTTP/1.1\n"
                                 "NotAHeader\n"
                                 "Host: localhost\n"
                                 "\n");
  EXPECT_EQ(req.headers.count("NotAHeader"), 0u);
  EXPECT_EQ(req.headers["Host"], "localhost");
}

TEST(HTTPParserTest, EmptyInput) {
  HTTPParser parser;
  HTTPRequest req = parser.parse("");

  EXPECT_TRUE(req.method.empty());
  EXPECT_TRUE(req.path.empty());
  EXPECT_TRUE(req.version.empty());
  EXPECT_TRUE(req.headers.empty());
}

// TEST(HTTPParserTest, KeepsHeaderNameAndDoesNotTrimValue){
TEST(HTTPParserTest, TrimsHeaderValue) {
  HTTPParser parser;
  HTTPRequest req = parser.parse("GET / HTTP/1.1\nConnection: keep-alive\n\n");
  EXPECT_EQ(req.headers.count("Connection"), 1u);
  EXPECT_EQ(req.headers["Connection"], "keep-alive");
}

TEST(HTTPParserTest, DoesNotRejectUnknownMethodOrVersion) {
  HTTPParser parser;
  HTTPRequest req = parser.parse("FOO / HTTP/9.9\n\n");
  EXPECT_EQ(req.method, "FOO");
  EXPECT_EQ(req.path, "/");
  EXPECT_EQ(req.version, "HTTP/9.9");
}

TEST(HTTPParserTest, ParsesCrlfHeaders){
    HTTPParser parser;
    HTTPRequest req=parser.parse(
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: keep-alive\r\n"
        "\r\n"
    );
    EXPECT_EQ(req.method,"GET");
    EXPECT_EQ(req.version,"HTTP/1.1");
    EXPECT_EQ(req.headers["Host"],"localhost");
    EXPECT_EQ(req.headers["Connection"],"keep-alive");
}
