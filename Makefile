# C++ Video Splitter Makefile

# Compiler settings
CXX = g++
CXXFLAGS = -O3 -std=c++17 -Wall -Wextra
TARGET = video_splitter
SRC = video_splitter.cpp

# Detect OS for executable extension
ifeq ($(OS),Windows_NT)
    EXT = .exe
else
    EXT =
endif

# Default target
all: $(TARGET)$(EXT)

# Build target
$(TARGET)$(EXT): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)$(EXT)
	@echo "✅ Build complete: $(TARGET)$(EXT)"

# Clean build artifacts
clean:
	rm -f $(TARGET)$(EXT)
	rm -f *.o
	rm -rf output_clips/
	@echo "🧹 Clean complete"

# Install to system (optional)
install: $(TARGET)$(EXT)
	sudo cp $(TARGET)$(EXT) /usr/local/bin/
	@echo "📦 Installed to /usr/local/bin/"

# Uninstall from system
uninstall:
	sudo rm -f /usr/local/bin/$(TARGET)$(EXT)
	@echo "🗑️  Uninstalled from system"

# Test build
test: $(TARGET)$(EXT)
	@echo "🧪 Testing build..."
	./$(TARGET)$(EXT) --help 2>/dev/null || echo "No --help, but binary exists"

# Development build with debug info
debug:
	$(CXX) -g -std=c++17 $(SRC) -o $(TARGET)_debug$(EXT)
	@echo "🐛 Debug build complete: $(TARGET)_debug$(EXT)"

# Help target
help:
	@echo "🎬 C++ Video Splitter Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  all      - Build optimized release version (default)"
	@echo "  debug    - Build with debug symbols"
	@echo "  clean    - Remove build artifacts"
	@echo "  test     - Test the build"
	@echo "  install  - Install to system (/usr/local/bin)"
	@echo "  uninstall- Remove from system"
	@echo "  help     - Show this help"
	@echo ""
	@echo "Usage:"
	@echo "  make              # Build release version"
	@echo "  make debug        # Build debug version"
	@echo "  make clean all    # Clean and rebuild"

.PHONY: all clean install uninstall test debug help