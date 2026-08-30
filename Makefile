CC     := gcc
CFLAGS := -O2 -Wall

SOLVERS := v1_src/v1

CSHARP_SRC := WebsiteReader.cs
WIN_STAMP := publish-win/.stamp

V1_SRC := $(wildcard v1_src/*.c)
V1_OBJ := $(V1_SRC:.c=.o)



.PHONY: all clean

all: $(WIN_STAMP) $(SOLVERS)

$(WIN_STAMP): $(CSHARP_SRC)
	dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -o publish-win
	touch $@

v1_src/v1: $(V1_OBJ)
	$(CC) $(CFLAGS) -o v1_src/v1 $(V1_OBJ)

v1_src/%.o: v1_src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(SOLVERS)
	rm -f v1_src/*.o
	rm -rf publish-win