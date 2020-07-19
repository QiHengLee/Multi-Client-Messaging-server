CFLAGS = -Wall -g
CC     = gcc $(CFLAGS)

all: bl_client bl_server

bl_client: bl_client.o server_funcs.o simpio.o util.o
	$(CC) -o bl_client bl_client.o server_funcs.o simpio.o util.o -pthread
	@echo bl_client is ready

bl_server: bl_server.o server_funcs.o util.o
	$(CC) -o bl_server bl_server.o server_funcs.o util.o
	@echo bl_server is ready

bl_client.o : bl_client.c blather.h
	$(CC) -c $<

bl_server.o : bl_server.c blather.h
	$(CC) -c $<

server_funcs.o : server_funcs.c blather.h
	$(CC) -c $<

simpio.o : simpio.c blather.h
	$(CC) -c $<

util.o : util.c blather.h
	$(CC) -c $<


############################################################
## TEST TARGETS
TEST_PROGRAMS = test_blather.sh test_blather_data.sh test_normalize.awk test_cat_sig.sh test_filter_semopen_bug.awk 

test : bl_client bl_server
	chmod u+rx test_*
	./test_blather.sh $(testnum)

clean-tests :
	cd test-data && \
	rm -f *.*

clean:
	@echo cleaning up object files
	rm -f bl_client bl_server *.o

# 'make zip' to create p2-code.zip for submission
AN=p2
SHELL  = /bin/bash
CWD    = $(shell pwd | sed 's/.*\///g')
zip : clean clean-tests
	rm -f $(AN)-code.zip
	cd .. && zip "$(CWD)/$(AN)-code.zip" -r "$(CWD)"
	@echo Zip created in $(AN)-code.zip
	@if (( $$(stat -c '%s' $(AN)-code.zip) > 10*(2**20) )); then echo "WARNING: $(AN)-code.zip seems REALLY big, check there are no abnormally large test files"; du -h $(AN)-code.zip; fi
	@if (( $$(unzip -t $(AN)-code.zip | wc -l) > 256 )); then echo "WARNING: $(AN)-code.zip has 256 or more files in it which may cause submission problems"; fi