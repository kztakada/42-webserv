CXX = c++
NAME = webserv

SRCS_DIR = srcs
SRCS := $(shell find srcs -type f -name '*.cpp')
OBJS_DIR = objs
OBJS := $(SRCS:%.cpp=$(OBJS_DIR)/%.o)
DEPENDENCIES := $(OBJS:.o=.d)

TEST_OBJS_DIR = objs_test

CXXFLAGS := -I$(SRCS_DIR) --std=c++98 -Wall -Wextra -Werror -pedantic


all: $(NAME)

$(OBJS_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -MMD -MP -o $@

$(TEST_OBJS_DIR)/%.o: %.cpp
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

re:
	$(MAKE) fclean
	$(MAKE) all

debug:
	$(MAKE) fclean
	$(MAKE) all CXXFLAGS='$(CXXFLAGS) -DDEBUG'

.PHONY: all re debug clean fclean

############ GooleTest ############

TEST_DIR    = unit_test
TESTER_NAME = ./tester

PYTHON ?= python3

GTEST_DIR   = ./google_test

GTEST_STAMP := $(GTEST_DIR)/.gtest_ready
GTEST_MAIN  := $(GTEST_DIR)/googletest-release-1.11.0/googletest/src/gtest_main.cc
GTEST_ALL   := $(GTEST_DIR)/gtest/gtest-all.cc
TEST_SRCS   := $(shell [ -d $(TEST_DIR) ] && find $(TEST_DIR) -type f -name '*.cpp')
# ユニットテストで必要な本体コード(.cpp)だけを指定してリンクする
# 例: make test TEST_PROD_SRCS='srcs/http/http_request.cpp'
TEST_PROD_SRCS ?= \
	srcs/network/ip_address.cpp \
	srcs/network/port_type.cpp \
	srcs/utils/path.cpp \
	srcs/utils/timestamp.cpp \
	srcs/utils/log.cpp \
	srcs/http/http_request.cpp \
	srcs/http/cgi_response.cpp \
	srcs/http/cgi_meta_variables.cpp \
	srcs/http/http_response.cpp \
		srcs/http/http_response_encoder.cpp \
	srcs/http/syntax.cpp \
	srcs/server/session/io_buffer.cpp \
	srcs/server/session/fd_session_controller.cpp \
	srcs/server/session/fd/cgi_pipe/cgi_pipe_fd.cpp \
	srcs/server/session/fd/tcp_socket/socket_address.cpp \
	srcs/server/session/fd/tcp_socket/tcp_connection_socket_fd.cpp \
	srcs/server/session/fd/tcp_socket/tcp_listen_socket_fd.cpp \
		srcs/server/session/fd_session/http_session/body_source.cpp \
		srcs/server/session/fd_session/http_session.cpp \
		srcs/server/session/fd_session/http_session/http_session_watch.cpp \
			srcs/server/http_processing_module/session_cgi_handler.cpp \
			srcs/server/http_processing_module/http_processing_module_utils.cpp \
			srcs/server/session/fd_session/http_session/session_context.cpp \
			srcs/server/session/fd_session/http_session/http_session_helpers.cpp \
		srcs/server/session/fd_session/http_session/http_session_event.cpp \
		srcs/server/session/fd_session/http_session/states/recv_request_state.cpp \
		srcs/server/session/fd_session/http_session/states/send_response_state.cpp \
		srcs/server/session/fd_session/http_session/states/execute_cgi_state.cpp \
		srcs/server/session/fd_session/http_session/states/close_wait_state.cpp \
		srcs/server/session/fd_session/http_session/http_session_prepare.cpp \
		srcs/server/session/fd_session/http_session/http_session_redirect.cpp \
	srcs/server/session/fd_session/cgi_session.cpp \
		srcs/server/session/fd_session/http_session/http_response_writer.cpp \
	srcs/server/session/fd_session/http_session/body_store.cpp \
	srcs/server/session/fd_session/http_session/http_request_handler.cpp \
	srcs/server/http_processing_module/request_dispatcher.cpp \
	srcs/server/session/fd_session/http_session/actions/execute_cgi_action.cpp \
	srcs/server/session/fd_session/http_session/actions/process_request_action.cpp \
	srcs/server/session/fd_session/http_session/actions/send_error_action.cpp \
	srcs/server/http_processing_module/request_processor.cpp \
	srcs/server/http_processing_module/request_processor/autoindex_renderer.cpp \
	srcs/server/http_processing_module/request_processor/action_handler_factory.cpp \
	srcs/server/http_processing_module/request_processor/handlers/internal_redirect_handler.cpp \
	srcs/server/http_processing_module/request_processor/handlers/redirect_external_handler.cpp \
	srcs/server/http_processing_module/request_processor/handlers/respond_error_handler.cpp \
	srcs/server/http_processing_module/request_processor/handlers/store_body_handler.cpp \
	srcs/server/http_processing_module/request_processor/handlers/static_autoindex_handler.cpp \
	srcs/server/http_processing_module/request_processor/config_text_loader.cpp \
	srcs/server/http_processing_module/request_processor/error_page_renderer.cpp \
	srcs/server/http_processing_module/request_processor/internal_redirect_resolver.cpp \
	srcs/server/http_processing_module/request_processor/static_file_responder.cpp \
	srcs/server/http_processing_module/request_router/location_directive.cpp \
	srcs/server/http_processing_module/request_router/location_routing.cpp \
	srcs/server/http_processing_module/request_router/resolved_request_context.cpp \
	srcs/server/http_processing_module/request_router/virtual_server.cpp \
	srcs/server/http_processing_module/request_router/request_router.cpp \
	srcs/server/config/location_directive_conf.cpp \
	srcs/server/config/parser/config_parser.cpp \
	srcs/server/config/virtual_server_conf.cpp \
	srcs/server/config/server_config.cpp \
	srcs/server/reactor/fd_watch.cpp \
	srcs/server/reactor/fd_event_reactor/epoll_reactor.cpp \
	srcs/server/reactor/fd_event_reactor/select_reactor.cpp \
	srcs/server/reactor/fd_event_reactor_factory.cpp
TEST_PROD_OBJS := $(TEST_PROD_SRCS:%.cpp=$(TEST_OBJS_DIR)/%.o)
TEST_OBJS      := $(TEST_PROD_OBJS) $(TEST_SRCS:%.cpp=$(TEST_OBJS_DIR)/%.o)
TEST_DEPENDENCIES \
         := $(TEST_OBJS:%.o=%.d)

-include $(TEST_DEPENDENCIES)


test: CXXFLAGS := -I$(SRCS_DIR) -I$(TEST_DIR) --std=c++11 -I$(GTEST_DIR) -g3 -fsanitize=address
test: $(GTEST_STAMP) $(TEST_OBJS)
	# Google Test require C++11
	$(CXX) $(CXXFLAGS) $(GTEST_MAIN) $(GTEST_ALL) \
		-I$(GTEST_DIR) -lpthread \
		$(TEST_OBJS) \
		-o $(TESTER_NAME)
	$(TESTER_NAME)

$(GTEST_STAMP):
	mkdir -p $(GTEST_DIR)
	rm -rf $(GTEST_DIR)/gtest $(GTEST_DIR)/googletest-release-1.11.0
	rm -rf googletest-release-1.11.0
	curl -OL https://github.com/google/googletest/archive/refs/tags/release-1.11.0.tar.gz
	tar -xvzf release-1.11.0.tar.gz googletest-release-1.11.0
	rm -rf release-1.11.0.tar.gz
	$(PYTHON) googletest-release-1.11.0/googletest/scripts/fuse_gtest_files.py $(GTEST_DIR)
	mv googletest-release-1.11.0 $(GTEST_DIR)
	@touch $(GTEST_STAMP)

clean_test:
	$(RM) $(TEST_OBJS) $(TEST_DEPENDENCIES)
	$(RM) $(TESTER_NAME)
	$(RM) -rf $(TEST_OBJS_DIR)
	$(RM) -rf ./-h
	$(RM) -rf $(GTEST_DIR)

.PHONY: test