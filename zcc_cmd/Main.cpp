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



int main(int argc, char** args)
{
	using namespace Zero;

	auto text = ReadFile("../test.zs");
	auto parser = Parser(text);
	auto root = parser.ParseFile();

	return 0;
}