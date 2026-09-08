CC      := gcc
CFLAGS  := -O2 -Wall
DEPFLAGS := -MMD -MP

CSHARP_SRC := WebsiteReader.cs
WIN_STAMP := publish-win/.stamp

SRC_DIRS := $(wildcard v*_src)
SOLVERS  := $(foreach dir,$(SRC_DIRS),$(dir)/$(dir:_src=))
OBJS     := $(foreach dir,$(SRC_DIRS),$(patsubst %.c,%.o,$(wildcard $(dir)/*.c)))
DEPS     := $(OBJS:.o=.d)

.PHONY: all clean

all: $(WIN_STAMP) $(SOLVERS)

$(WIN_STAMP): $(CSHARP_SRC)
	dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -o publish-win
	touch $@

define SOLVER_RULE
$(1)/$(1:_src=): $$(patsubst %.c,%.o,$$(wildcard $(1)/*.c))
	$$(CC) $$(CFLAGS) -o $$@ $$^
endef
$(foreach dir,$(SRC_DIRS),$(eval $(call SOLVER_RULE,$(dir))))

%.o: %.c
	$(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

-include $(DEPS)

clean:
	rm -f $(SOLVERS)
	rm -f v*_src/*.o v*_src/*.d
	rm -rf publish-win