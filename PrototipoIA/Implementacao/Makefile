# Bancada de teste — build com make (macOS ou Linux). O CMakeLists.txt é o build oficial do grupo (Linux/WSL2).
#   make            compila build/bancada, build/testes e build/testes_memoria
#   make test       roda os testes (os de memória auxiliar ficam num executável próprio)
#   make asan       compila e roda os testes com ASan + UBSan (build-asan/)
#   make rapido     roda a bancada em escala 0,1, confere com o SQLite e gera as tabelas
#   make clean

BUILD   ?= build
CXX     ?= c++
CXXSTD  := -std=c++20
OPT     ?= -O2 -g
WARN    := -Wall -Wextra -Wpedantic -Wshadow
CPPFLAGS += -Isrc -Itests
CXXFLAGS += $(CXXSTD) $(OPT) $(WARN) -DCBD_FLAGS='"$(CXXSTD) $(OPT)"'

LIB_SRC   := $(wildcard src/common/*.cpp src/layer4/*.cpp src/layer3/*.cpp src/layer2/*.cpp)
LIB_OBJ   := $(LIB_SRC:%.cpp=$(BUILD)/%.o)
BENCH_OBJ := $(BUILD)/src/layer1/bench.o
TEST_SRC  := $(wildcard tests/*.cpp)
TEST_OBJ  := $(TEST_SRC:%.cpp=$(BUILD)/%.o)
MEM_SRC   := $(wildcard tests/memoria/*.cpp) tests/mini_main.cpp
MEM_OBJ   := $(MEM_SRC:%.cpp=$(BUILD)/%.o)

.PHONY: all test asan rapido clean

all: $(BUILD)/bancada $(BUILD)/testes $(BUILD)/testes_memoria

$(BUILD)/bancada: $(LIB_OBJ) $(BENCH_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/testes: $(LIB_OBJ) $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/testes_memoria: $(LIB_OBJ) $(MEM_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(TEST_OBJ) $(MEM_OBJ): CXXFLAGS += -Wno-missing-field-initializers

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(LIB_OBJ:.o=.d) $(BENCH_OBJ:.o=.d) $(TEST_OBJ:.o=.d) $(MEM_OBJ:.o=.d)

test: $(BUILD)/testes $(BUILD)/testes_memoria
	./$(BUILD)/testes
	./$(BUILD)/testes_memoria

asan:
	$(MAKE) BUILD=build-asan OPT="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer" build-asan/testes build-asan/testes_memoria
	ASAN_OPTIONS=detect_leaks=0 ./build-asan/testes
	ASAN_OPTIONS=detect_leaks=0 ./build-asan/testes_memoria

rapido: $(BUILD)/bancada
	./$(BUILD)/bancada --rapido --saida saida_rapida
	python3 -I tools/reference.py saida_rapida
	python3 -I tools/compare.py saida_rapida
	python3 -I tools/tabelas.py saida_rapida

clean:
	rm -rf build build-asan
