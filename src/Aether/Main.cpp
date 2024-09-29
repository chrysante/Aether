int macOSMain(int, char const**);

__attribute__((weak)) int main(int argc, char const** argv) {
    return macOSMain(argc, argv);
}
