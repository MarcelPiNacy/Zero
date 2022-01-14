#include <zcc_core/AST.hpp>
#include <zcc_core/Parser.hpp>



std::string ReadFile(const char* path)
{
	std::string r = "";
	FILE* file = nullptr;
	fopen_s(&file, path, "rb");
	if (file == nullptr)
		return r;
	fseek(file, 0, SEEK_END);
	r.resize((size_t)ftell(file));
	fseek(file, 0, SEEK_SET);
	fread(&r[0], 1, r.size(), file);
	fclose(file);
	return r;
}



uint32_t Test(const char* path)
{
	auto text = ReadFile(path);
	auto parser = Zero::Parser(text);
	auto root = parser.ParseFile();
	return 0;
}



int main(int argc, char** args)
{
	Test("ControlFlow.txt");
	return 0;
}