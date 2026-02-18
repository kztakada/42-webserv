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

re:
	$(MAKE) fclean
	$(MAKE) all

.PHONY: all re clean fclean
