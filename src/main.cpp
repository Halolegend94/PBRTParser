
#include "PBRTParser.h"
#include <fstream>

int main(int argc, char** argv){
	std::cout << "Loading scene\n";
	auto obj = ygl::load_scene(argv[1]);
	std::cout << "make bvh..\n";
	auto bvh = ygl::make_bvh(obj);
	auto tp = ygl::trace_params();
	tp.nsamples = 32;
	tp.parallel = true;
	tp.min_depth = 2;
	tp.max_depth = 3;
	tp.resolution = 80;
	std::cout << "tracing..\n";
	auto im = ygl::trace_image(obj, obj->cameras[0], bvh, tp);
	ygl::save_image("codetraced.png", im, 0, 2);
	exit(1);
	
	if (argc < 3)
	{
		printf("Usage: command <input_scene_file> <output_scene_file>\n");
		exit(1);
	}
	auto parser = PBRTParser(argv[1]);
	ygl::scene *scn;
	try {
		scn = parser.parse();
	}
	catch (std::exception ex) {
		std::cout << ex.what() << std::endl;
		return 1;
	}

	try {
		std::cout << "Conversion ended. Saving obj to file..\n";
		auto so = ygl::save_options();
		so.skip_missing = false;
		ygl::save_scene(argv[2], scn, so);
	}
	catch (std::exception ex) {
		std::cout << ex.what() << "\n";
	}
	
	return 0;
}
