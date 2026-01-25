CXX = c++
NAME = webserv

SRCS_DIR = srcs
SRCS := $(shell find srcs -type f -name '*.cpp')
OBJS_DIR = objs
OBJS := $(SRCS:%.cpp=$(OBJS_DIR)/%.o)
DEPENDENCIES := $(OBJS:.o=.d)

CXXFLAGS := -I$(SRCS_DIR) --std=c++98 -Wall -Wextra -Werror -pedantic


all: $(NAME)

$(OBJS_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -MMD -MP -o $@

-include $(DEPENDENCIES)


$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(NAME) $^

clean:
	$(RM) $(OBJS) $(DEPENDENCIES)
	$(RM) -r $(OBJS_DIR)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: all re clean fclean

############ GooleTest ############

TEST_DIR    = unit_test
TESTER_NAME = ./tester

PYTHON ?= python3

GTEST_DIR   = ./google_test

GTEST       := $(GTEST_DIR)/gtest $(GTEST_DIR)/googletest-release-1.11.0
GTEST_MAIN  := $(GTEST_DIR)/googletest-release-1.11.0/googletest/src/gtest_main.cc
GTEST_ALL   := $(GTEST_DIR)/gtest/gtest-all.cc
TEST_SRCS   := $(shell [ -d $(TEST_DIR) ] && find $(TEST_DIR) -type f -name '*.cpp')
# ユニットテストで必要な本体コード(.cpp)だけを指定してリンクする
# 例: make test TEST_PROD_SRCS='srcs/http/http_request.cpp'
TEST_PROD_SRCS ?= srcs/http/http_request.cpp srcs/http/syntax.cpp
TEST_PROD_OBJS := $(TEST_PROD_SRCS:%.cpp=$(OBJS_DIR)/%.o)
TEST_OBJS      := $(TEST_PROD_OBJS) $(TEST_SRCS:%.cpp=$(OBJS_DIR)/%.o)
TEST_DEPENDENCIES \
         := $(TEST_OBJS:%.o=%.d)

-include $(TEST_DEPENDENCIES)


test: CXXFLAGS := -I$(SRCS_DIR) -I$(TEST_DIR) --std=c++11 -I$(GTEST_DIR) -g3 -fsanitize=address
test: $(GTEST) $(TEST_OBJS)
	# Google Test require C++11
	$(CXX) $(CXXFLAGS) $(GTEST_MAIN) $(GTEST_ALL) \
		-I$(GTEST_DIR) -lpthread \
		$(TEST_OBJS) \
		-o $(TESTER_NAME)
	$(TESTER_NAME)

$(GTEST):
	mkdir -p $(GTEST_DIR)
	rm -rf $(GTEST_DIR)/gtest $(GTEST_DIR)/googletest-release-1.11.0
	rm -rf googletest-release-1.11.0
	curl -OL https://github.com/google/googletest/archive/refs/tags/release-1.11.0.tar.gz
	tar -xvzf release-1.11.0.tar.gz googletest-release-1.11.0
	rm -rf release-1.11.0.tar.gz
	$(PYTHON) googletest-release-1.11.0/googletest/scripts/fuse_gtest_files.py $(GTEST_DIR)
	mv googletest-release-1.11.0 $(GTEST_DIR)

clean_test:
	$(RM) $(TEST_OBJS) $(TEST_DEPENDENCIES)
	$(RM) $(TESTER_NAME)
	$(RM) -rf $(OBJS_DIR)
	$(RM) -rf ./-h
	$(RM) -rf $(GTEST_DIR)

.PHONY: test $(GTEST)