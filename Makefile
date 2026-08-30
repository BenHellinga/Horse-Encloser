CC     := gcc
CFLAGS := -O2 -Wall

CSHARP_SRC := WebsiteReader.cs
WIN_STAMP := publish-win/.stamp

# every vN_src directory becomes a solver target of the same name, e.g. v1_src/v1
SRC_DIRS := $(wildcard v*_src)
SOLVERS  := $(foreach dir,$(SRC_DIRS),$(dir)/$(dir:_src=))



.PHONY: all clean

all: $(WIN_STAMP) $(SOLVERS)

$(WIN_STAMP): $(CSHARP_SRC)
	dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -o publish-win
	touch $@

# pattern rule: v1_src/v1 depends on all .o files in v1_src, same for v2_src/v2, etc.
define SOLVER_RULE
$(1)/$(1:_src=): $$(patsubst %.c,%.o,$$(wildcard $(1)/*.c))
	$$(CC) $$(CFLAGS) -o $$@ $$^
endef
$(foreach dir,$(SRC_DIRS),$(eval $(call SOLVER_RULE,$(dir))))

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(SOLVERS)
	rm -f v*_src/*.o
	rm -rf publish-win