CXXFLAGS =  -Wall -ggdb -std=c++1z -I../../thirdparty/all/include -I../../thirdparty/gzstream -I../../thirdparty/CRoaring/include
LDFLAGS  =  -L../../thirdparty/all/lib -L../../thirdparty/gzstream -L../../thirdparty/CRoaring/build -lroaring -lsdsl -lpthread -lz -lgzstream -ljellyfish-2.0 

UNAME := $(shell uname)

CPP_FILES := howdesbt.cc \
	cmd_make_bf.cc cmd_cluster.cc cmd_build_sbt.cc cmd_query.cc \
             cmd_version.cc \
             query.cc \
             bloom_tree.cc bloom_filter.cc bit_vector.cc file_manager.cc \
             bit_utilities.cc utilities.cc support.cc
OBJ_FILES := $(addprefix ./,$(notdir $(CPP_FILES:.cc=.o)))


all: CXXFLAGS += -DNDEBUG -O3 
	
all: clean howdesbt

howdesbt: $(OBJ_FILES)
ifeq ($(UNAME),Darwin)
	$(CXX) -o $@ $^ $(LDFLAGS) -lc++fs -lc++experimental
else 
	$(CXX) -o $@ $^ $(LDFLAGS) -lstdc++fs
endif	

%.o: %.cc
	$(CXX) -c -o $@ $^ $(CXXFLAGS)

clean: cleano
	rm -f howdesbt

cleano: 
	rm -f *.o
