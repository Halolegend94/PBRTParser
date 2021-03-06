#include "PBRTParser.h"

// Build a parser for the scene pointed by "filename"
PBRTParser::PBRTParser(std::string filename) {
	this->lexers.push_back(std::shared_ptr<PBRTLexer>(new PBRTLexer(filename)));
	this->scn = new ygl::scene();
	this->fill_parameter_to_type_mapping();
};


//
// parse
//
ygl::scene *PBRTParser::parse() {
	this->advance();
	this->execute_preworld_directives();
	this->execute_world_directives();
	return scn;
}

// =====================================================================================
//                           TYPE CHECKING
// =====================================================================================

//
// check_synonym
// some types are synonyms, transform them to default.
//
std::string PBRTParser::check_synonyms(std::string s) {
	if (s == "point")
		return std::string("point3");
	if (s == "normal")
		return std::string("normal3");
	if (s == "vector")
		return std::string("vector3");
	if (s == "color")
		return std::string("rgb");

	return s;
}

//
// fill_parameter_to_type_mapping
//
#define MP std::make_pair<std::string, std::vector<std::string>>

void PBRTParser::fill_parameter_to_type_mapping() {
	// camera parameters
	parameterToType.insert(MP("frameaspectratio", { "float" }));
	parameterToType.insert(MP("lensradius", { "float" }));
	parameterToType.insert(MP("focaldistance", { "float" }));
	parameterToType.insert(MP("fov", { "float" }));
	// film
	parameterToType.insert(MP("xresolution", { "integer" }));
	parameterToType.insert(MP("yresolution", { "integer" }));
	parameterToType.insert(MP("lensradius", { "float" }));
	// curve
	parameterToType.insert(MP("p", { "point3" }));
	parameterToType.insert(MP("type", { "string" }));
	parameterToType.insert(MP("N", { "normal3" }));
	parameterToType.insert(MP("splitdepth", { "integer" }));
	parameterToType.insert(MP("width", { "float" }));
	// triangle mesh
	parameterToType.insert(MP("indices", { "integer" }));
	parameterToType.insert(MP("P", { "point3" }));
	parameterToType.insert(MP("uv", { "float" }));
	// lights
	parameterToType.insert(MP("scale", { "spectrum", "rgb", "float" }));
	parameterToType.insert(MP("L", { "spectrum", "rgb", "blackbody" }));
	parameterToType.insert(MP("mapname", { "string" }));
	parameterToType.insert(MP("I", { "spectrum" }));
	parameterToType.insert(MP("from", { "point3" }));
	parameterToType.insert(MP("twosided", { "bool" }));
	// materials
	parameterToType.insert(MP("Kd", { "spectrum", "rgb", "texture" }));
	parameterToType.insert(MP("Ks", { "spectrum", "rgb", "texture" }));
	parameterToType.insert(MP("Kr", { "spectrum", "rgb", "texture" }));
	parameterToType.insert(MP("reflect", { "spectrum", "rgb", "texture" }));
	parameterToType.insert(MP("Kt", { "spectrum", "rgb", "texture" }));
	parameterToType.insert(MP("transmit", { "spectrum", "rgb", "texture" }));
	parameterToType.insert(MP("roughness", { "float", "texture" }));
	parameterToType.insert(MP("eta", { "spectrum", "rgb", "texture" }));
	parameterToType.insert(MP("index", { "float" }));
	parameterToType.insert(MP("amount", { "float", "rgb" }));
	parameterToType.insert(MP("namedmaterial1", { "string" }));
	parameterToType.insert(MP("namedmaterial2", { "string" }));
	parameterToType.insert(MP("bumpmap", { "texture" }));
	// textures
	parameterToType.insert(MP("filename", { "string" }));
	parameterToType.insert(MP("value", { "float", "spectrum", "rgb" }));
	parameterToType.insert(MP("uscale", { "float" }));
	parameterToType.insert(MP("vscale", { "float" }));
	parameterToType.insert(MP("tex1", { "texture", "float", "spectrum", "rgb" }));
	parameterToType.insert(MP("tex2", { "texture", "float", "spectrum", "rgb" }));
}

//
// check_param_type
// returns true if the check goes well, false if the parameter is unknown,
// throws an exception if the type differs from the expected one.
//
bool PBRTParser::check_param_type(std::string par, std::string parsedType) {
	auto p = parameterToType.find(par);
	if (p == parameterToType.end()) {
		return false;
	}
	auto v = p->second;
	if (std::find(v.begin(), v.end(), parsedType) == v.end()) {
		// build expected type string
		std::stringstream exp;
		for (auto x : v)
			exp << x << "/";
		std::string expstr = exp.str();
		expstr = expstr.substr(0, expstr.length() - 1);
		throw_syntax_exception("Parameter '" + par + "' expects a " + expstr + " type. ");
	}
	return true;
}

//
// advance
// Fetches the next lexeme (token)
//
void PBRTParser::advance() {
	try {
		this->lexers.at(0)->next_lexeme();
	}
	catch (InputEndedException ex) {
		this->lexers.erase(this->lexers.begin());
		if (lexers.size() == 0) {
			throw InputEndedException();
		}
		// after being restored, we still need to flush away the included filename.
		// See execute_Include() for more info.
		this->advance();
	}
};

//
// Remove all the tokens till the next directive
//
void PBRTParser::ignore_current_directive() {
	this->advance();
	while (this->current_token().type != LexemeType::IDENTIFIER)
		this->advance();
}

//
// execute_preworld_directives
//
void PBRTParser::execute_preworld_directives() {
    // When this method starts executing, the first token must be an Identifier of
	// a directive.

	// parse scene wide rendering options until the WorldBegin statement is met.
	while (!(this->current_token().type == LexemeType::IDENTIFIER &&
		this->current_token().value =="WorldBegin")) {

		if (this->current_token().type != LexemeType::IDENTIFIER)
			throw_syntax_exception("Identifier expected, got " + this->current_token().value + " instead.");

		// Scene-Wide rendering options
		else if (this->current_token().value =="Camera") {
			this->execute_Camera();
		}
		else if (this->current_token().value =="Film"){
			this->execute_Film();
		}

		else if (this->current_token().value =="Include") {
			this->execute_Include();
		}
		else if (this->current_token().value =="Translate") {
			this->execute_Translate();
		}
		else if (this->current_token().value =="Transform") {
			this->execute_Transform();
		}
		else if (this->current_token().value =="ConcatTransform") {
			this->execute_ConcatTransform();
		}
		else if (this->current_token().value =="Scale") {
			this->execute_Scale();
		}
		else if (this->current_token().value =="Rotate") {
			this->execute_Rotate();
		}
		else if (this->current_token().value =="LookAt"){
			this->execute_LookAt();
		}
		else {
			warning_message("Ignoring " + this->current_token().value + " directive..");
			this->ignore_current_directive();
		}
	}
}

//
// execute_world_directives
//
void PBRTParser::execute_world_directives() {
	this->gState.CTM = ygl::identity_mat4f;
	this->advance();
	// parse scene wide rendering options until the WorldBegin statement is met.
	while (!(this->current_token().type == LexemeType::IDENTIFIER &&
		this->current_token().value =="WorldEnd")) {
		this->execute_world_directive();
	}
}

//
// execute_world_directive
// NOTE: This has been separated from execute_world_directives because it will be called
// also inside ObjectBlock.
//
void PBRTParser::execute_world_directive() {

	if (this->current_token().type != LexemeType::IDENTIFIER)
		throw_syntax_exception("Identifier expected, got " + this->current_token().value + " instead.");

	if (this->current_token().value =="Include") {
		this->execute_Include();
	}
	else if (this->current_token().value =="Translate") {
		this->execute_Translate();
	}
	else if (this->current_token().value =="Transform") {
		this->execute_Transform();
	}
	else if (this->current_token().value =="ConcatTransform") {
		this->execute_ConcatTransform();
	}
	else if (this->current_token().value =="Scale") {
		this->execute_Scale();
	}
	else if (this->current_token().value =="Rotate") {
		this->execute_Rotate();
	}
	else if (this->current_token().value =="LookAt") {
		this->execute_LookAt();
	}
	else if (this->current_token().value =="AttributeBegin") {
		this->execute_AttributeBegin();
	}
	else if (this->current_token().value =="TransformBegin") {
		this->execute_TransformBegin();
	}
	else if (this->current_token().value =="AttributeEnd") {
		this->execute_AttributeEnd();
	}
	else if (this->current_token().value =="TransformEnd") {
		this->execute_TransformEnd();
	}
	else if (this->current_token().value =="Shape") {
		this->execute_Shape();
	}
	else if (this->current_token().value =="ObjectBegin") {
		this->execute_ObjectBlock();
	}
	else if (this->current_token().value =="ObjectInstance") {
		this->execute_ObjectInstance();
	}
	else if (this->current_token().value =="LightSource") {
		this->execute_LightSource();
	}
	else if (this->current_token().value =="AreaLightSource") {
		this->execute_AreaLightSource();
	}
	else if (this->current_token().value =="Material") {
		this->execute_Material(false);
	}
	else if (this->current_token().value =="MakeNamedMaterial") {
		this->execute_Material(true);
	}
	else if (this->current_token().value =="NamedMaterial") {
		this->execute_NamedMaterial();
	}
	else if (this->current_token().value =="Texture") {
		this->execute_Texture();
	}
	else {
		warning_message("Ignoring " + this->current_token().value + " directive..");
		this->ignore_current_directive();
	}
}

//
// get_unique_id
//
std::string PBRTParser::get_unique_id(CounterID id) {
	char buff[300];
	unsigned int val;
	std::string st = "";

	if (id == CounterID::shape)
		st = "s_", val = shapeCounter++;
	else if (id == CounterID::shape_group)
		st = "sg_", val = shapeGroupCounter++;
	else if (id == CounterID::instance)
		st = "i_", val = instanceCounter++;
	else if (id == CounterID::material)
		st = "m_", val = materialCounter++;
	else if (id == CounterID::environment)
		st = "e_", val = envCounter++;
	else
		st = "t_", val = textureCounter++;

	sprintf(buff, "%s%u", st.c_str(), val);
	return std::string(buff);
};

// ----------------------------------------------------------------------------
//                      PARAMETERS PARSING
// ----------------------------------------------------------------------------


//
// parse_parameter
// Fills the PBRTParameter structure with type name and value of the
// parameter. It is upon to the caller to convert "void * value" to
// the correct pointer type (by reading the name and type).
//
std::shared_ptr<PBRTParameter> PBRTParser::parse_parameter(){

	std::shared_ptr<PBRTParameter> par (new PBRTParameter);

	auto valueToString = [](std::string x)->std::string {return x; };
	auto valueToFloat = [](std::string x)->float {return atof(x.c_str()); };
	auto valueToInt = [](std::string x)->int {return atoi(x.c_str()); };

	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_exception("Expected a string with type and name of a parameter.");
    
	auto tokens = split(this->current_token().value);
    
	par->type = check_synonyms(std::string(tokens[0]));
	par->name = tokens[1];

	check_param_type(par->name, par->type);
	this->advance();

	// now, according to type, we parse the value
	if (par->type == "string" || par->type == "texture") {
		std::vector<std::string> *vals = new std::vector<std::string>();
		this->parse_value<std::string, LexemeType::STRING>(vals, valueToString);
		par->value = (void *)vals;
	}

	else if (par->type == "float") {
		std::vector<float> *vals = new std::vector<float>();
		this->parse_value<float, LexemeType::NUMBER>(vals, valueToFloat);
		par->value = (void *)vals;
	}

	else if (par->type == "integer") {
		std::vector<int> *vals = new std::vector<int>();
		this->parse_value<int, LexemeType::NUMBER>(vals, valueToInt);
		par->value = (void *)vals;
	}
	else if (par->type == "bool") {
		std::vector<std::string> *vals = new std::vector<std::string>();
		this->parse_value<std::string, LexemeType::STRING>(vals, valueToString);
		for (auto v : *vals) {
			if (v != "false" && v != "true")
				throw_syntax_exception("A value diffeent from true and false "\
					"has been given to a bool type parameter.");
		}
		par->value = (void *)vals;
	}
	// now we come at a special case of arrays of vec3f
	else if (par->type == "point3" || par->type == "normal3"|| par->type == "rgb") {
		std::unique_ptr<std::vector<float>> vals(new::std::vector<float>());
		this->parse_value<float, LexemeType::NUMBER>(vals.get(), valueToFloat);
		if (vals->size() % 3 != 0)
			throw_syntax_exception("Wrong number of values given.");
		std::vector<ygl::vec3f> *vectors = new std::vector<ygl::vec3f>();
		int count = 0;
		while (count < vals->size()) {
			ygl::vec3f v;
			for (int i = 0; i < 3; i++, count++) {
				v[i] = vals->at(count);
			}
			vectors->push_back(v);
		}
		par->value = (void *)vectors;
	}
	else if (par->type == "spectrum") {
		// spectrum data can be given using a file or directly as list
		std::vector<ygl::vec2f> samples;
		if (this->current_token().type == LexemeType::STRING) {
			// filename given
			std::string fname = this->current_path() + "/" + this->current_token().value;
			this->advance();
			if (!load_spectrum_from_file(fname, samples))
				throw_syntax_exception("Error loading spectrum data from file.");
		}else {
			// step 1: read raw data
			std::unique_ptr<std::vector<float>> vals(new::std::vector<float>());
			this->parse_value<float, LexemeType::NUMBER>(vals.get(), valueToFloat);
			// step 2: pack it in list of vec2f (lambda, val)
			if (vals->size() % 2 != 0)
				throw_syntax_exception("Wrong number of values given.");
			int count = 0;
			while (count < vals->size()) {
				auto lamb = vals->at(count++);
				auto v = vals->at(count++);
				samples.push_back({ lamb, v });
			}
		}
		// step 3: convert to rgb and store in a vector (because it simplifies interface to get data)
		std::vector<ygl::vec3f> *data = new std::vector<ygl::vec3f>();
		data->push_back(spectrum_to_rgb(samples));
		par->value = (void *)data;
		par->type = std::string("rgb");
	}
	else if (par->type == "blackbody") {
		// step 1: read raw data
		std::unique_ptr<std::vector<float>> vals(new::std::vector<float>());
		this->parse_value<float, LexemeType::NUMBER>(vals.get(), valueToFloat);
		// step 2: pack it in list of vec2f (lambda, val)
		if (vals.get()->size() != 2) // NOTE: actually must be % 2 != 0
			throw_syntax_exception("Wrong number of values given.");
		
		// step 3: convert to rgb and store in a vector (because it simplifies interface to get data)
		std::vector<ygl::vec3f> *data = new std::vector<ygl::vec3f>();
		data->push_back(blackbody_to_rgb(vals.get()->at(0), vals.get()->at(1)));
		par->value = (void *)data;
		par->type = std::string("rgb");
	}
	else {
		throw_syntax_exception("Cannot able to parse the value: type '" + par->type + "' not supported.");
	}
	return par;
}

//
// parse_parameters
//
void PBRTParser::parse_parameters(std::vector<std::shared_ptr<PBRTParameter>> &pars) {
	// read parameters
	while (this->current_token().type != LexemeType::IDENTIFIER) {
		pars.push_back(this->parse_parameter());
		
	}
}

// ------------------ END PARAMETER PARSING --------------------------------

//
// execute_Include
//
void PBRTParser::execute_Include() {
	this->advance();
	// now read the file to include
	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_exception("Expected the name of the file to be included.");

	std::string fileToBeIncl = concatenate_paths(this->current_path(), this->current_token().value);

	// call advance here on the current lexer is dangerous. It could end the parsing too soon.
	// better call it in advance() method, directly on the lexter after being
	// restored.
	this->lexers.insert(this->lexers.begin(), std::shared_ptr<PBRTLexer>(new PBRTLexer(fileToBeIncl)));
	this->advance(); // this advance is on the new Lexer
}

// ------------------------------------------------------------------------------------
//                               TRANSFORMATIONS
// ------------------------------------------------------------------------------------

//
// execute_Translate()
//
void PBRTParser::execute_Translate() {
	this->advance();
	ygl::vec3f transl_vec{ };

	for (int i = 0; i < 3; i++) {
		if (this->current_token().type != LexemeType::NUMBER)
			throw_syntax_exception("Expected a float value.");
		transl_vec[i] = atof(this->current_token().value.c_str());
		this->advance();
	}
	
	auto transl_mat = ygl::frame_to_mat(ygl::translation_frame(transl_vec));
	this->gState.CTM = this->gState.CTM * transl_mat;
}

//
// execute_Scale()
//
void PBRTParser::execute_Scale(){
	this->advance();
	ygl::vec3f scale_vec;

	for (int i = 0; i < 3; i++) {
		if (this->current_token().type != LexemeType::NUMBER)
			throw_syntax_exception("Expected a float value.");
		scale_vec[i] = atof(this->current_token().value.c_str());
		this->advance();
	}
	
	auto scale_mat = ygl::frame_to_mat(ygl::scaling_frame(scale_vec));
	this->gState.CTM =  this->gState.CTM * scale_mat;
}

//
// execute_Rotate()
//
void PBRTParser::execute_Rotate() {
	this->advance();
	float angle;
	ygl::vec3f rot_vec;

	if (this->current_token().type != LexemeType::NUMBER)
		throw_syntax_exception("Expected a float value for 'angle' parameter of Rotate directive.");
	angle = atof(this->current_token().value.c_str());
	angle = angle * (ygl::pif / 180.0f);
	
	this->advance();

	for (int i = 0; i < 3; i++) {
		if (this->current_token().type != LexemeType::NUMBER)
			throw_syntax_exception("Expected a float value.");
		rot_vec[i] = atof(this->current_token().value.c_str());
		this->advance();
	}
	auto rot_mat = ygl::frame_to_mat(ygl::rotation_frame(rot_vec, angle));
	this->gState.CTM = this->gState.CTM * rot_mat;
}

//
// execute_LookAt()
//
void PBRTParser::execute_LookAt(){
	this->advance();
	std::vector<ygl::vec3f> vects(3); //eye, look and up
	
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			if (this->current_token().type != LexemeType::NUMBER)
				throw_syntax_exception("Expected a float value.");
			vects[i][j] = atof(this->current_token().value.c_str());
			this->advance();
		}
	}

	auto fm = ygl::lookat_frame(vects[0], vects[1], vects[2]);
	fm.x = -fm.x;
	fm.z = -fm.z;
	auto mm = ygl::frame_to_mat(fm);
	this->defaultFocus = ygl::length(vects[0] - vects[1]);
	this->gState.CTM = this->gState.CTM * ygl::inverse(mm); // inverse here because pbrt boh
}

//
// execute_Transform()
//
void PBRTParser::execute_Transform() {
	this->advance();
	std::vector<float> vals;
	this->parse_value<float, LexemeType::NUMBER>(&vals, [](std::string x)->float {return atof(x.c_str()); });
	ygl::mat4f nCTM;
	if (vals.size() != 16)
		throw_syntax_exception("Wrong number of values given. Expected a 4x4 matrix.");

	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			nCTM[i][j] = vals.at(i * 4 + j);
		}
	}
	gState.CTM = nCTM;
}

//
// execute_ConcatTransform()
//
void PBRTParser::execute_ConcatTransform() {
	this->advance();
	std::vector<float> vals;
	this->parse_value<float, LexemeType::NUMBER>(&vals, [](std::string x)->float {return atof(x.c_str()); });
	ygl::mat4f nCTM;
	if (vals.size() != 16)
		throw_syntax_exception("Wrong number of values given. Expected a 4x4 matrix.");

	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			nCTM[i][j] = vals.at(i * 4 + j);
		}
	}
	gState.CTM = gState.CTM * nCTM;
}

// --------------------------------------------------------------------------
//                  SCENE-WIDE RENDERING OPTIONS
// --------------------------------------------------------------------------

//
// execute_Camera
// Parse camera information.
// NOTE: only support perspective camera for now.
//
void PBRTParser::execute_Camera() {
	this->advance();

	ygl::camera *cam = new ygl::camera;
	cam->aspect = defaultAspect;
	cam->aperture = 0;
	cam->yfov = 90.0f * ygl::pif / 180;
	cam->focus = defaultFocus;
	char buff[100];
	sprintf(buff, "c%lu", scn->cameras.size());
	cam->name = std::string(buff);
	
	// CTM defines world to camera transformation
	cam->frame = ygl::mat_to_frame(ygl::inverse(this->gState.CTM));
	cam->frame.z = -cam->frame.z;
	
	// Parse the camera parameters
	// First parameter is the type
	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_exception("Expected type string.");
	std::string camType = this->current_token().value;
	// RESTRICTION: only perspective camera is supported
	if (camType != "perspective")
		throw_syntax_exception("Only perspective camera type is supported.");
	this->advance();

	std::vector<std::shared_ptr<PBRTParameter>> params;
	this->parse_parameters(params);

	int i_frameaspect = find_param("frameaspectratio", params);
	if (i_frameaspect >= 0)
		cam->aspect = params[i_frameaspect]->get_first_value<float>();
	
	int i_fov = find_param("fov", params);
	if (i_fov >= 0) {
		cam->yfov = (params[i_fov]->get_first_value<float>())*ygl::pif / 180;
	}
		
	scn->cameras.push_back(cam);
}

//
// execute_Film
//
void PBRTParser::execute_Film() {
	this->advance();

	// First parameter is the type
	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_exception("Expected type string.");
	std::string filmType = this->current_token().value;
	
	if (filmType != "image")
		throw_syntax_exception("Only image \"film\" is supported.");

	this->advance();
	int xres = 0;
	int yres = 0;

	std::vector<std::shared_ptr<PBRTParameter>> params;
	this->parse_parameters(params);

	int i_xres = find_param("xresolution", params);
	if (i_xres >= 0)
		xres = params[i_xres]->get_first_value<int>();

	int i_yres = find_param("yresolution", params);
	if (i_yres >= 0)
		yres = params[i_yres]->get_first_value<int>();

	if (xres && yres) {
		auto asp = ((float)xres) / ((float)yres);
		if (asp < 1)
			asp = 1; // TODO: vertical images
		this->defaultAspect = asp;

		for (auto cam : scn->cameras)
			cam->aspect = this->defaultAspect;
	}
}

// -----------------------------------------------------------------------------
//                         DESCRIBING THE SCENE
// -----------------------------------------------------------------------------

//
// execute_AttributeBegin
// Enter into a inner graphics scope
//
void PBRTParser::execute_AttributeBegin() {
	this->advance();
	// save the current state
	stateStack.push_back(this->gState);
}

//
// execute_AttributeEnd
//
void PBRTParser::execute_AttributeEnd() {
	this->advance();
	if (stateStack.size() == 0) {
		throw_syntax_exception("AttributeEnd instruction unmatched with AttributeBegin.");
	}
	this->gState = stateStack.back();
	stateStack.pop_back();
}

//
// execute_TransformBegin
//
void PBRTParser::execute_TransformBegin() {
	this->advance();
	// save the current CTM
	CTMStack.push_back(this->gState.CTM);
}

//
// execute_TransformEnd
//
void PBRTParser::execute_TransformEnd() {
	this->advance();
	// save the current CTM
	if (CTMStack.size() == 0) {
		throw_syntax_exception("TranformEnd instruction unmatched with TransformBegin.");
	}
	this->gState.CTM = CTMStack.back();
	CTMStack.pop_back();
}

// ---------------------------------------------------------------------------------
//                                  SHAPES
// ---------------------------------------------------------------------------------

//
// parse_cube
// DEBUG FUNCTION
//
void PBRTParser::parse_cube(ygl::shape *shp) {
	// DEBUG: this is a debug function
	while (this->current_token().type != LexemeType::IDENTIFIER)
		this->advance();
	ygl::make_uvcube(shp->quads, shp->pos, shp->norm, shp->texcoord, 1);
}

//
// parse_triangle_mesh
//
void PBRTParser::parse_trianglemesh(ygl::shape *shp) {

	bool indicesCheck = false;
	bool PCheck = false;

	std::vector<std::shared_ptr<PBRTParameter>> params;
	this->parse_parameters(params);
	// vertices
	int i_p = find_param("P", params);
	if (i_p >= 0) {
		auto data = (std::vector<ygl::vec3f> *) params[i_p]->value;
		for (auto p : *data) {
			shp->pos.push_back(p);
		}
		PCheck = true;

	}
	// normals
	int i_N = find_param("N", params);
	if (i_N >= 0) {
		auto data = (std::vector<ygl::vec3f> *) params[i_N]->value;
		for (auto p : *data) {
			shp->norm.push_back(p);
		}
	}
	// indices
	int i_indices = find_param("indices", params);
	if (i_indices >= 0) {
		std::vector<int> *data = (std::vector<int> *) params[i_indices]->value;
		if (data->size() % 3 != 0) {
			delete shp;
			throw_syntax_exception("The number of triangle vertices must be multiple of 3.");
		}
			

		int count = 0;
		while (count < data->size()) {
			ygl::vec3i triangle;
			triangle[0] = data->at(count++);
			triangle[1] = data->at(count++);
			triangle[2] = data->at(count++);
			shp->triangles.push_back(triangle);
		}
		indicesCheck = true;
	}

	int i_uv = find_param("uv", params);
	if (i_uv == -1)
		i_uv = find_param("st", params);
	if(i_uv >= 0){
		std::vector<float> *data = (std::vector<float> *)params[i_uv]->value;
		int j = 0;
		while (j < data->size()) {
			ygl::vec2f uv;
			uv[0] = data->at(j++);
			uv[1] = data->at(j++);
			shp->texcoord.push_back(uv);
		}
	}
	
	// TODO: materials parameters overriding
	// Single material parameters (e.g. Kd, Ks) can be directly specified on a shape
	// overriding (for this shape) the value of the current material in the graphical state.

	// TODO: solve problem with normals
	if (shp->norm.size() == 0);
		my_compute_normals(shp->triangles, shp->pos, shp->norm, true);

	if (!(indicesCheck && PCheck)) {
		delete shp;
		throw_syntax_exception("Missing indices or positions in triangle mesh specification.");
	}	
}

//
// execute_Shape
//
void PBRTParser::execute_Shape() {
	this->advance();

	// parse the shape name
	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_exception("Expected shape name.");
	std::string shapeName = this->current_token().value;
	this->advance();
	
	ygl::shape *shp = new ygl::shape();
	shp->name = get_unique_id(CounterID::shape);
	// add material to shape
	if (!gState.mat) {
		// since no material was defined, empty material is created
		warning_message("No material defined for this shape. Empty material created..");
		ygl::material *nM = new ygl::material();
		nM->name = get_unique_id(CounterID::material);
		shp->mat = nM;
		scn->materials.push_back(nM);
	}
	else {
		if (!gState.mat->addedInScene) {
			gState.mat->addedInScene = true;
			scn->materials.push_back(gState.mat->mat);
		}
		shp->mat = gState.mat->mat;
	}
	// TODO: handle when shapes override some material properties

	if (this->gState.areaLight.active) {
		shp->mat->ke = gState.areaLight.L;
		shp->mat->double_sided = gState.areaLight.twosided;
	}

	else if (shapeName == "trianglemesh")
		this->parse_trianglemesh(shp);

	else if (shapeName == "cube")
		this->parse_cube(shp);
	
	else if (shapeName == "sphere") {
		std::vector<std::shared_ptr<PBRTParameter>> params;
		this->parse_parameters(params);
		float radius = 1;
		int i_rad = find_param("radius", params);
		if (i_rad >= 0) {
			radius = params[i_rad]->get_first_value<float>();
		}
		ygl::make_uvspherizedcube(shp->quads, shp->pos, shp->norm, shp->texcoord, 4, radius);
	}

	else if (shapeName == "disk") {
		this->ignore_current_directive();
		shp->pos.push_back({ 5, 0, 5 }); shp->pos.push_back({ -5, 0, 5 }); 
		shp->pos.push_back({ -5, 0, -5 }); shp->pos.push_back({ 5, 0, -5 });
		shp->quads.push_back({ 0, 1, 2, 3 });
		
	}
	else if (shapeName == "plymesh"){
		std::shared_ptr<PBRTParameter> par = this->parse_parameter();

		if (par->name != "filename") {
			delete shp;
			throw_syntax_exception("Expected ply file path.");
		}
			
		std::string fname = this->current_path() + "/" + par->get_first_value<std::string>();

		if (!parse_ply(fname, shp)){
			delete shp;
			throw_syntax_exception("Error parsing ply file: " + fname);
		}

		while (this->current_token().type != LexemeType::IDENTIFIER)
			this->advance();
	
	}
	else {
		this->ignore_current_directive();
		delete shp;
		warning_message("Ignoring shape " + shapeName + ".");
		return;
	}

	// handle texture coordinate scaling
	for (int i = 0; i < shp->texcoord.size(); i++) {
		shp->texcoord[i].x *= gState.uscale;
		shp->texcoord[i].y *= gState.vscale;
	}

	// add shp in scene
	ygl::shape_group *sg = new ygl::shape_group;
	sg->shapes.push_back(shp);
	sg->name = get_unique_id(CounterID::shape_group);

	if (this->inObjectDefinition) {
		shapesInObject.push_back(sg);
	}
	else {
		scn->shapes.push_back(sg);
		// add a single instance directly to the scene
		ygl::instance *inst = new ygl::instance();
		inst->shp = sg;
		// TODO: check the correctness of this
		// NOTE: current transformation matrix is used to set the object to world transformation for the shape.
		inst->frame = ygl::mat_to_frame(this->gState.CTM);
		inst->name = get_unique_id(CounterID::instance);
		scn->instances.push_back(inst);
	}
}

// ------------------- END SHAPES --------------------------------------------------

//
// execute_ObjectBlock
//
void PBRTParser::execute_ObjectBlock() {
	if (this->inObjectDefinition)
		throw_syntax_exception("Cannot define an object inside another object.");
	this->execute_AttributeBegin(); // it will execute advance() too
	this->inObjectDefinition = true;
	this->shapesInObject.clear();
	int start = this->lexers[0]->get_line();

	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_exception("Expected object name as a string.");
	//NOTE: the current transformation matrix defines the transformation from
	//object space to instance's coordinate space
	std::string objName = this->current_token().value;
	this->advance();

	while (!(this->current_token().type == LexemeType::IDENTIFIER &&
		this->current_token().value == "ObjectEnd")){
		this->execute_world_directive();
	}

	auto it = nameToObject.find(objName);
	if (it == nameToObject.end()){
		nameToObject.insert(std::make_pair(objName, std::shared_ptr<DeclaredObject>\
			(new DeclaredObject(shapesInObject, this->gState.CTM))));
	}		
	else {
		it->second = std::shared_ptr<DeclaredObject>(new DeclaredObject(this->shapesInObject, this->gState.CTM));
	}
		
	this->inObjectDefinition = false;
	this->execute_AttributeEnd();
}

//
// execute_ObjectInstance
//
void PBRTParser::execute_ObjectInstance() {
	this->advance();
	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_exception("Expected object name as a string.");
	//NOTE: the current transformation matrix defines the transformation from
	//instance space to world coordinate space
	std::string objName = this->current_token().value;
	this->advance();

	auto obj = nameToObject.find(objName);
	if (obj == nameToObject.end())
		throw_syntax_exception("Object name not found.");

	auto shapes = obj->second->sg;
	if (shapes.size() > 0) {
		ygl::mat4f finalCTM = this->gState.CTM * obj->second->CTM;

		for (auto shape : shapes) {
			ygl::instance *inst = new ygl::instance();
			if (!(obj->second->addedInScene)) {
				scn->shapes.push_back(shape);
			}
			inst->shp = shape;
			inst->frame = ygl::mat_to_frame(finalCTM);
			inst->name = get_unique_id(CounterID::instance);
			scn->instances.push_back(inst);
		}
		if (!(obj->second->addedInScene)) {
			obj->second->addedInScene = true;
		}
	}
}

// --------------------------------------------------------------------------
//                        LIGHTS
// --------------------------------------------------------------------------

//
// execute_LightSource
//
void PBRTParser::execute_LightSource() {
	this->advance();
	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_exception("Expected lightsource type as a string.");
	std::string lightType = this->current_token().value;
	this->advance();

	if (lightType == "point")
		this->parse_PointLight();
	else if (lightType == "infinite")
		this->parse_InfiniteLight();
	else if (lightType == "distant"){
		// TODO: implement real distant light
		this->parse_InfiniteLight();
	}else
		throw_syntax_exception("Light type " + lightType + " not supported.");
}

//
// parse_InfiniteLight
//
void PBRTParser::parse_InfiniteLight() {
	// TODO: conversion from spectrum to rgb
	ygl::vec3f scale { 1, 1, 1 };
	ygl::vec3f L { 1, 1, 1 };
	std::string mapname;

	std::vector<std::shared_ptr<PBRTParameter>> params;
	this->parse_parameters(params);

	int i_scale = find_param("scale", params);
	if (i_scale >= 0) {
		scale = params[i_scale]->get_first_value<ygl::vec3f>();
	}
	int i_L = find_param("L", params);
	if (i_L >= 0) {
		L = params[i_L]->get_first_value<ygl::vec3f>();
	}
	int i_map = find_param("mapname", params);
	if (i_map >= 0) {
		mapname = params[i_map]->get_first_value<std::string>();
	}
	
	ygl::environment *env = new ygl::environment;
	env->name = get_unique_id(CounterID::environment);
	env->ke = scale * L;
	
	const ygl::vec3f x_axis = {1, 0, 0};
	const ygl::vec3f y_axis = {0, 1, 0};
	auto rm = ygl::frame_to_mat(ygl::rotation_frame(x_axis, 90 * ygl::pif / 180));
	rm *= ygl::frame_to_mat(ygl::rotation_frame(y_axis, 180 * ygl::pif / 180));
	auto fm = ygl::mat_to_frame(gState.CTM * rm);
	fm.z = -fm.z;
	env->frame = fm;

	if (mapname.length() > 0) {
		ygl::texture *txt = new ygl::texture;
		txt->name = get_unique_id(CounterID::texture);
		load_texture(txt, mapname, false);
		scn->textures.push_back(txt);
		env->ke_txt_info = new ygl::texture_info();
		env->ke_txt = txt;
	}
	scn->environments.push_back(env);
}

//
// parse_PointLight
//
void PBRTParser::parse_PointLight() {
	ygl::vec3f scale{ 1, 1, 1 };
	ygl::vec3f I{ 1, 1, 1 };
	ygl::vec3f point;

	std::vector<std::shared_ptr<PBRTParameter>> params;
	this->parse_parameters(params);

	int i_scale = find_param("scale", params);
	if (i_scale >= 0) {
		scale = params[i_scale]->get_first_value<ygl::vec3f>();
	}
	int i_I = find_param("I", params);
	if (i_I >= 0) {
		I = params[i_I]->get_first_value<ygl::vec3f>();
	}
	int i_from = find_param("from", params);
	if (i_from >= 0) {
		point = params[i_from]->get_first_value<ygl::vec3f>();
	}

	ygl::shape_group *sg = new ygl::shape_group;
	sg->name = get_unique_id(CounterID::shape_group);

	ygl::shape *lgtShape = new ygl::shape;
	lgtShape->name = get_unique_id(CounterID::shape);
	lgtShape->pos.push_back(point);
	lgtShape->points.push_back(0);
	// default radius
	lgtShape->radius.push_back(1.0f);

	sg->shapes.push_back(lgtShape);

	ygl::material *lgtMat = new ygl::material;
	lgtMat->ke = I * scale;
	lgtShape->mat = lgtMat;
	lgtMat->name = get_unique_id(CounterID::material);
	scn->materials.push_back(lgtMat);

	scn->shapes.push_back(sg);
	ygl::instance *inst = new ygl::instance;
	inst->shp = sg;

	inst->frame = ygl::mat_to_frame(gState.CTM);
	inst->name = get_unique_id(CounterID::instance);
	scn->instances.push_back(inst);
}

//
// execute_AreaLightSource
//
void PBRTParser::execute_AreaLightSource() {
	this->advance();
	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_exception("Expected lightsource type as a string.");
	
	std::string areaLightType = this->current_token().value;
	// TODO: check type
	this->advance();

	ygl::vec3f scale{ 1, 1, 1 };
	ygl::vec3f L{ 1, 1, 1 };
	bool twosided = false;

	std::vector<std::shared_ptr<PBRTParameter>> params;
	this->parse_parameters(params);
	
	int i_scale = find_param("scale", params);
	if (i_scale >= 0) {
		scale = params[i_scale]->get_first_value<ygl::vec3f>();
	}
	int i_L = find_param("L", params);
	if (i_L >= 0) {
		L = params[i_L]->get_first_value<ygl::vec3f>();
	}
	
	int i_ts = find_param("twosided", params);
	if (i_ts >= 0) {
		twosided =params[i_ts]->get_first_value<std::string>() == "true" ? true : false;
	}
	
	this->gState.areaLight.active = true;
	this->gState.areaLight.L = L;
	this->gState.areaLight.twosided = twosided;
}
// --------------------------- END LIGHTS ---------------------------------------

// ------------------------------------------------------------------------------
//                             MATERIALS
// ------------------------------------------------------------------------------


void PBRTParser::execute_Material(bool namedMaterial) {
	this->advance();
	std::string materialName; // used only if namedMaterial
	std::string materialType;

	std::shared_ptr<DeclaredMaterial> dmat(new DeclaredMaterial);
	dmat->mat = new ygl::material;
	dmat->mat->name = get_unique_id(CounterID::material);

	if (namedMaterial) {
		if (this->current_token().type != LexemeType::STRING)
			throw_syntax_exception("Expected material name as string.");

		materialName = this->current_token().value;
		if (gState.nameToMaterial.find(materialName) != gState.nameToMaterial.end())
			throw_syntax_exception("A material with the specified name already exists.");
		this->advance();
	}
	else {
		if (this->current_token().type != LexemeType::STRING)
			throw_syntax_exception("Expected material type as a string.");
		materialType = this->current_token().value;
		this->advance();
	}

	std::vector<std::shared_ptr<PBRTParameter>> params{};
	this->parse_parameters(params);

	if (namedMaterial) {
		int i_mtype = find_param("type", params);
		if (i_mtype < 0)
			throw_syntax_exception("Expected type of named material.");
		else
			materialType = params[i_mtype]->get_first_value<std::string>();
	}

	// bump is common to every material
	int i_bump = find_param("bump", params);
	if (i_bump >= 0) {
		auto txtName = params[i_bump]->get_first_value<std::string>();
		auto dbump = texture_lookup(txtName, true);
		dmat->mat->bump_txt = dbump->txt;
		gState.uscale = dbump->uscale;
		gState.vscale = dbump->vscale;
	}

	if (materialType == "matte")
		this->parse_material_matte(dmat, params);
	else if (materialType == "metal")
		this->parse_material_metal(dmat, params);
	else if (materialType == "mix")
		this->parse_material_mix(dmat, params);
	else if (materialType == "plastic")
		this->parse_material_plastic(dmat, params);
	else if (materialType == "mirror")
		this->parse_material_mirror(dmat, params);
	else if (materialType == "uber")
		this->parse_material_uber(dmat, params);
	else if (materialType == "translucent")
		this->parse_material_translucent(dmat, params);
	else if (materialType == "glass")
		this->parse_material_glass(dmat, params);
	else if (materialType == "substrate")
		this->parse_material_substrate(dmat, params);
	else {
		warning_message("Material '" + materialType + "' not supported. Ignoring and using 'matte'..");
		this->parse_material_matte(dmat, params);
	}
	if (namedMaterial)
		gState.nameToMaterial.insert(std::make_pair(materialName, dmat));
	else
		this->gState.mat = dmat;
}

//
// execute_NamedMaterial
//
void PBRTParser::execute_NamedMaterial() {
	this->advance();
	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_exception("Expected material name string.");
	std::string materialName = this->current_token().value;
	this->advance();
	auto it = gState.nameToMaterial.find(materialName);
	if (it == gState.nameToMaterial.end())
		throw_syntax_exception("No material with the specified name.");
	this->gState.mat = it->second;
}

//
// parse_material_matte
//
void PBRTParser::parse_material_matte(std::shared_ptr<DeclaredMaterial> &dmat, std::vector<std::shared_ptr<PBRTParameter>> &params) {
	dmat->mat->kd = { 0.5f, 0.5f, 0.5f };
	dmat->mat->rs = 1;
	
	int i_kd = find_param("Kd", params);
	if (i_kd >= 0) {
		set_k_property(params[i_kd], dmat->mat->kd, &(dmat->mat->kd_txt));
	}
}

//
// parse_material_uber
//
void PBRTParser::parse_material_uber(std::shared_ptr<DeclaredMaterial> &dmat, std::vector<std::shared_ptr<PBRTParameter>> &params) {
	dmat->mat->kd = { 0.25f, 0.25f, 0.25f };
	dmat->mat->ks = { 0.25f, 0.25f, 0.25f };
	dmat->mat->kr = { 0, 0, 0 };
	dmat->mat->rs = 0.01f;

	int i_kd = find_param("Kd", params);
	if (i_kd >= 0) {
		set_k_property(params[i_kd], dmat->mat->kd, &(dmat->mat->kd_txt));
	}

	int i_ks = find_param("Ks", params);
	if (i_ks >= 0) {
		set_k_property(params[i_ks], dmat->mat->ks, &(dmat->mat->ks_txt));
	}

	int i_kr = find_param("Kr", params);
	if (i_kr >= 0) {
		set_k_property(params[i_kr], dmat->mat->kr, &(dmat->mat->kr_txt));
	}

	int i_rs = find_param("roughness", params);
	if (i_rs >= 0) {
		if (params[i_rs]->type == "texture") {
			auto txtName = params[i_rs]->get_first_value<std::string>();
			dmat->mat->rs_txt = texture_lookup(txtName, true)->txt;
			dmat->mat->rs = 1;	
		}
		else {
			dmat->mat->rs = params[i_rs]->get_first_value<float>();
		}
	}
	
}

//
// parse_material_translucent
//
void PBRTParser::parse_material_translucent(std::shared_ptr<DeclaredMaterial> &dmat, std::vector<std::shared_ptr<PBRTParameter>> &params) {
	dmat->mat->kd = { 0.25f, 0.25f, 0.25f };
	dmat->mat->ks = { 0.25f, 0.25f, 0.25f };
	dmat->mat->kr = { 0.5f, 0.5f, 0.5f };
	dmat->mat->kt = { 0.5f, 0.5f, 0.5f };
	dmat->mat->rs = 0.1f;
	
	int i_kr = find_param("Kr", params);
	if (i_kr >= 0) {
		set_k_property(params[i_kr], dmat->mat->kr, &(dmat->mat->kr_txt));
	}

	int i_kd = find_param("Kd", params);
	if (i_kd >= 0) {
		set_k_property(params[i_kd], dmat->mat->kd, &(dmat->mat->kd_txt));
	}

	int i_ks = find_param("Ks", params);
	if (i_ks >= 0) {
		set_k_property(params[i_ks], dmat->mat->ks, &(dmat->mat->ks_txt));
	}

	int i_kt = find_param("Kt", params);
	if (i_kt >= 0) {
		set_k_property(params[i_kt], dmat->mat->kt, &(dmat->mat->kt_txt));
	}

	int i_rs = find_param("roughness", params);
	if (i_rs >= 0) {
		if (params[i_rs]->type == "texture") {
			auto txtName = params[i_rs]->get_first_value<std::string>();
			dmat->mat->rs_txt = texture_lookup(txtName, true)->txt;
			dmat->mat->rs = 1;
		}
		else {
			dmat->mat->rs = params[i_rs]->get_first_value<float>();
		}
	}
}

//
// parser_material_metal
//
void PBRTParser::parse_material_metal(std::shared_ptr<DeclaredMaterial> &dmat, std::vector<std::shared_ptr<PBRTParameter>> &params) {
	ygl::vec3f eta{ 0.5, 0.5, 0.5 };
	ygl::texture *etaTexture = nullptr;
	ygl::vec3f k { 0.5, 0.5, 0.5 };
	ygl::texture *kTexture = nullptr;
	dmat->mat->rs = 0.01;

	int i_eta = find_param("eta", params);
	if (i_eta >= 0) {
		set_k_property(params[i_eta], eta, &(etaTexture));
	}
	int i_k = find_param("k", params);
	if (i_k >= 0) {
		set_k_property(params[i_k], k, &(kTexture));
	}
	
	int i_rs = find_param("roughness", params);
	if (i_rs >= 0) {
		if (params[i_rs]->type == "texture") {
			auto txtName = params[i_rs]->get_first_value<std::string>();
			dmat->mat->rs_txt = texture_lookup(txtName, true)->txt;
			dmat->mat->rs = 1;
		}
		else {
			dmat->mat->rs = params[i_rs]->get_first_value<float>();
		}
	}
	dmat->mat->ks = ygl::fresnel_metal(1, eta, k);
}

//
// parse_material_mirror
//
void PBRTParser::parse_material_mirror(std::shared_ptr<DeclaredMaterial> &dmat, std::vector<std::shared_ptr<PBRTParameter>> &params) {
	dmat->mat->kr = { 0.9f, 0.9f, 0.9f };
	dmat->mat->rs = 0;
	int i_kr = find_param("Kr", params);
	if (i_kr >= 0) {
		set_k_property(params[i_kr], dmat->mat->kr, &(dmat->mat->kr_txt));
	}
}

//
// parse_material_plastic
//
void PBRTParser::parse_material_plastic(std::shared_ptr<DeclaredMaterial> &dmat, std::vector<std::shared_ptr<PBRTParameter>> &params) {
	dmat->mat->kd = { 0.25, 0.25, 0.25 };
	dmat->mat->ks = { 0.25, 0.25, 0.25 };
	dmat->mat->rs = 0.1;
	
	int i_kd = find_param("Kd", params);
	if (i_kd >= 0) {
		set_k_property(params[i_kd], dmat->mat->kd, &(dmat->mat->kd_txt));
	}

	int i_ks = find_param("Ks", params);
	if (i_ks >= 0) {
		set_k_property(params[i_ks], dmat->mat->ks, &(dmat->mat->ks_txt));
	}
}

//
// parse_material_substrate
//
void PBRTParser::parse_material_substrate(std::shared_ptr<DeclaredMaterial> &dmat, std::vector<std::shared_ptr<PBRTParameter>> &params) {
	dmat->mat->kd = { 0.5, 0.5, 0.5 };
	dmat->mat->ks = { 0.5, 0.5, 0.5 };
	dmat->mat->rs = 0;
	
	int i_kd = find_param("Kd", params);
	if (i_kd >= 0) {
		set_k_property(params[i_kd], dmat->mat->kd, &(dmat->mat->kd_txt));
	}

	int i_ks = find_param("Ks", params);
	if (i_ks >= 0) {
		set_k_property(params[i_ks], dmat->mat->ks, &(dmat->mat->ks_txt));
	}
}

//
// parse_material_glass
//
void PBRTParser::parse_material_glass(std::shared_ptr<DeclaredMaterial> &dmat, std::vector<std::shared_ptr<PBRTParameter>> &params) {
	dmat->mat->ks = { 0.04f, 0.04f, 0.04f };
	dmat->mat->kt = { 1, 1, 1 };
	dmat->mat->rs = 0.1;
	
	int i_ks = find_param("Ks", params);
	if (i_ks >= 0) {
		set_k_property(params[i_ks], dmat->mat->ks, &(dmat->mat->ks_txt));
	}

	int i_kt = find_param("Kt", params);
	if (i_kt >= 0) {
		set_k_property(params[i_kt], dmat->mat->kt, &(dmat->mat->kt_txt));
	}
}

// --------------------------------------------------------------------------------
// The following functions are used to mix materials
// TODO: test them. Actually I should check if PBRT implements this in a similar way
// --------------------------------------------------------------------------------

//
// blend_textures
// linearly blend two textures.
//
ygl::texture* PBRTParser::blend_textures(ygl::texture *txt1, ygl::texture *txt2, float amount) {

	auto ts1 = TextureSupport(txt1);
	auto ts2 = TextureSupport(txt2);
	ygl::texture *txt = nullptr;

	auto scale_single_texture = [&amount](TextureSupport &txtin)->ygl::texture* {
		ygl::texture *txt = new ygl::texture();
		txt->ldr = ygl::image4b(txtin.width, txtin.height);
		for (int w = 0; w < txtin.width; w++) {
			for (int h = 0; h < txtin.height; h++) {
				txt->ldr.at(w, h) = ygl::float_to_byte(txtin.at(w, h)*(1 - amount));
			}
		}
		return txt;
	};

	if (!txt1 && !txt2)
		return nullptr;
	else if (!txt1) {
		txt = scale_single_texture(ts2);
	}
	else if (!txt2) {
		txt = scale_single_texture(ts1);
	}
	else {
		txt = new ygl::texture();

		int width = ts1.width > ts2.width ? ts1.width : ts2.width;
		int height = ts1.height > ts2.height ? ts1.height : ts2.height;

		auto img = ygl::image4b(width, height);

		for (int w = 0; w < width; w++) {
			for (int h = 0; h < height; h++) {
				int w1 = w % ts1.width;
				int h1 = h % ts1.height;
				int w2 = w % ts2.width;
				int h2 = h % ts2.height;

				auto newPixel = ts1.at(w1, h1)* amount + ts2.at(w2, h2)* (1 - amount);
				img.at(w, h) = ygl::float_to_byte(newPixel);
			}
		}
		txt->ldr = img;
	}
	txt->name = get_unique_id(CounterID::texture);
	txt->path = textureSavePath + "/" + txt->name + ".png";
	scn->textures.push_back(txt);
	return txt;
}

//
// parse_material_mix
//
void PBRTParser::parse_material_mix(std::shared_ptr<DeclaredMaterial> &dmat, std::vector<std::shared_ptr<PBRTParameter>> &params) {
	
	float amount = 0.5f;
	std::string m1, m2;
	int i_am = find_param("amount", params);
	if (i_am >= 0) {
		amount = params[i_am]->get_first_value<float>();
	}
	int i_m1 = find_param("namedmaterial1", params);
	if (i_m1 >= 0) {
		m1 = params[i_m1]->get_first_value<std::string>();
	}
	else {
		throw_syntax_exception("Missing namedmaterial1.");
	}
	int i_m2 = find_param("namedmaterial2", params);
	if (i_m2 >= 0) {
		m2 = params[i_m2]->get_first_value<std::string>();
	}
	else {
		throw_syntax_exception("Missing namedmaterial2.");
	}

	auto mat1 = material_lookup(m1, false)->mat;
	auto mat2 = material_lookup(m2, false)->mat;

	// blend vectors
	dmat->mat->kd = (1 - amount)*mat2->kd + amount * mat1->kd;
	dmat->mat->kr = (1 - amount)*mat2->kr + amount * mat1->kr;
	dmat->mat->ks = (1 - amount)*mat2->ks + amount * mat1->ks;
	dmat->mat->kt = (1 - amount)*mat2->kt + amount * mat1->kt;
	dmat->mat->rs = (1 - amount)*mat2->rs + amount * mat1->rs;
	// blend textures
	dmat->mat->kd_txt = blend_textures(mat1->kd_txt, mat2->kd_txt, amount);
	dmat->mat->kr_txt = blend_textures(mat1->kr_txt, mat2->kr_txt, amount);
	dmat->mat->ks_txt = blend_textures(mat1->ks_txt, mat2->ks_txt, amount);
	dmat->mat->kt_txt = blend_textures(mat1->kt_txt, mat2->kt_txt, amount);
	dmat->mat->rs_txt = blend_textures(mat1->rs_txt, mat2->rs_txt, amount);
	dmat->mat->bump_txt = blend_textures(mat1->bump_txt, mat2->bump_txt, amount);
	dmat->mat->disp_txt = blend_textures(mat1->disp_txt, mat2->disp_txt, amount);
	dmat->mat->norm_txt = blend_textures(mat1->norm_txt, mat2->norm_txt, amount);

	dmat->mat->op = mat1->op * amount + mat2->op *(1 - amount);
}

//
// set_k_property
// Convenience function to set kd, ks, kt, kr from parsed parameter
//
void PBRTParser::set_k_property(std::shared_ptr<PBRTParameter> &par, ygl::vec3f &k, ygl::texture **txt) {
	if (par->type == "texture") {
		auto declTexture = texture_lookup(par->get_first_value<std::string>(), true);
		*txt = declTexture->txt;
		gState.uscale = declTexture->uscale;
		gState.vscale = declTexture->vscale;
		k = { 1, 1, 1 };
	}
	else {
		k = par->get_first_value<ygl::vec3f>();
	}
}

// -----------------------------------------------------------------------------
//                                TEXTURES
// -----------------------------------------------------------------------------

//
// load_texture image from file
//
void PBRTParser::load_texture(ygl::texture *txt, std::string &filename, bool flip) {
	auto completePath = this->current_path() + "/" + filename;
	auto ext = ygl::path_extension(filename);
	auto name = ygl::path_basename(filename);
	ext = ext == ".exr" ? ".hdr" : ext;
	txt->path = textureSavePath + "/" + name + ext;
	if (ext == ".hdr") {
		auto im = ygl::load_image4f(completePath);
		txt->hdr = flip ? flip_image(im) : im;
	}
	else {
		auto im = ygl::load_image4b(completePath);
		txt->ldr = flip ? flip_image(im) : im;
	}
}


void PBRTParser::parse_imagemap_texture(std::shared_ptr<DeclaredTexture> &dt) {
	
	std::string filename = "";
	// read parameters
	std::vector<std::shared_ptr<PBRTParameter>> params{};
	this->parse_parameters(params);

	int i_u = find_param("uscale", params);
	if (i_u >= 0)
		dt->uscale = params[i_u]->get_first_value<float>();

	int i_v = find_param("vscale", params);
	if (i_v >= 0)
		dt->vscale = params[i_v]->get_first_value<float>();
	
	int i_fn = find_param("filename", params);
	if (i_fn >= 0) {
		filename = params[i_fn]->get_first_value<std::string>();
	}
	else {
		throw_syntax_exception("No texture filename provided.");
	}

	if (dt->uscale < 1) dt->uscale = 1;
	if (dt->vscale < 1) dt->vscale = 1;

	load_texture(dt->txt, filename);
}

//
// parse_constant_texture
//
void PBRTParser::parse_constant_texture(std::shared_ptr<DeclaredTexture> &dt) {
	

	ygl::vec3f value{ 1, 1, 1 };
	// read parameters
	std::vector<std::shared_ptr<PBRTParameter>> params{};
	this->parse_parameters(params);

	int i_v = find_param("value", params);
	if (i_v >= 0) {
		if (params[i_v]->type == "float") {
			auto v = params[i_v]->get_first_value<float>();
			value.x = v;
			value.y = v;
			value.z = v;
		}
		else {
			value = params[i_v]->get_first_value<ygl::vec3f>();
		}
	}
	dt->txt->ldr = make_constant_image(value);
}

//
// parse_checkerboard_texture
//
void PBRTParser::parse_checkerboard_texture(std::shared_ptr<DeclaredTexture> &dt) {

	ygl::vec4f tex1{ 0,0,0, 255 }, tex2{ 1,1,1, 255};

	// read parameters
	std::vector<std::shared_ptr<PBRTParameter>> params{};
	this->parse_parameters(params);

	int i_u = find_param("uscale", params);
	if (i_u >= 0)
		dt->uscale = params[i_u]->get_first_value<float>();

	int i_v = find_param("vscale", params);
	if (i_v >= 0)
		dt->vscale = params[i_v]->get_first_value<float>();

	int i_txt1 = find_param("tex1", params);
	if (i_txt1 >= 0) {
		if (params[i_txt1]->type == "float") {
			auto v = params[i_txt1]->get_first_value<float>();
			tex1.x = v;
			tex1.y = v;
			tex1.z = v;
		}
		else {
			auto v = params[i_txt1]->get_first_value<ygl::vec3f>();
			tex1.x = v.x;
			tex1.y = v.y;
			tex1.z = v.z;
		}
	}
	int i_txt2 = find_param("tex2", params);
	if (i_txt2 >= 0) {
		if (params[i_txt2]->type == "float") {
			auto v = params[i_txt2]->get_first_value<float>();
			tex2.x = v;
			tex2.y = v;
			tex2.z = v;
		}
		else {
			auto v = params[i_txt2]->get_first_value<ygl::vec3f>();
			tex2.x = v.x;
			tex2.y = v.y;
			tex2.z = v.z;
		}
	}

	// hack
	if (dt->uscale < 0) dt->uscale = 1;
	if (dt->vscale < 0) dt->vscale = 1;

	dt->txt->ldr = ygl::make_checker_image(128, 128, 64, float_to_byte(tex1), float_to_byte(tex2));
}

//
// parse_scale_texture
//
void PBRTParser::parse_scale_texture(std::shared_ptr<DeclaredTexture> &dt) {

	bool free_ytex1 = false;
	bool free_ytex2 = false;
	ygl::texture *ytex1;
	ygl::texture *ytex2;

	// read parameters
	std::vector<std::shared_ptr<PBRTParameter>> params{};
	this->parse_parameters(params);
	
	// first get the first texture
	int i_tex1 = find_param("tex1", params);
	if (i_tex1 == -1)
		throw_syntax_exception("Impossible to create scale texture, missing tex1.");

	if (params[i_tex1]->type == "texture") {
		ytex1 = texture_lookup(params[i_tex1]->get_first_value<std::string>(), false)->txt;
	}
	else {
		free_ytex1 = true;
		ytex1 = new ygl::texture();
		if (params[i_tex1]->type == "float")
			ytex1->ldr = make_constant_image(params[i_tex1]->get_first_value<float>());
		else if (params[i_tex1]->type == "rgb")
			ytex1->ldr = make_constant_image(params[i_tex1]->get_first_value<ygl::vec3f>());
		else
			throw_syntax_exception("Texture argument 'tex1' type not recognised in scale texture.");
	}
	// retrieve the textures
	int i_tex2 = find_param("tex2", params);
	if (i_tex2 == -1) {
		if (free_ytex1)
			delete ytex1;
		throw_syntax_exception("Impossible to create scale texture, missing tex2.");
	}
		
	if (params[i_tex2]->type == "texture") {
		ytex2 = texture_lookup(params[i_tex2]->get_first_value<std::string>(), false)->txt;
	}
	else {
		ytex2 = new ygl::texture();
		free_ytex2 = true;
		if (params[i_tex2]->type == "float")
			ytex2->ldr = make_constant_image(params[i_tex2]->get_first_value<float>());
		else if (params[i_tex2]->type == "rgb")
			ytex2->ldr = make_constant_image(params[i_tex2]->get_first_value<ygl::vec3f>());
		else {
			if (free_ytex1)
				delete ytex1;
			if (free_ytex2)
				delete ytex2;
			throw_syntax_exception("Texture argument 'tex2' type not recognised in scale texture.");
		}
			
	}
	auto ts1 = TextureSupport(ytex1);
	auto ts2 = TextureSupport(ytex2);
	// NOTE: tiling of the smaller texture is performed here. Check if pbrt does the same

	int width = ts1.width > ts2.width ? ts1.width : ts2.width;
	int height = ts1.height > ts2.height ? ts1.height : ts2.height;
	auto img = ygl::image4b(width, height);

	for (int w = 0; w < width; w++) {
		for (int h = 0; h < height; h++) {
			int w1 = w % ts1.width;
			int h1 = h % ts1.height;
			int w2 = w % ts2.width;
			int h2 = h % ts2.height;

			auto newPixel = ts1.at(w1, h1)* ts2.at(w2, h2);
			img.at(w, h) = ygl::float_to_byte(newPixel);
		}
	}
	dt->txt->ldr = img;

	if (free_ytex1)
		delete ytex1;
	if (free_ytex2)
		delete ytex2;

	int i_u = find_param("uscale", params);
	if (i_u >= 0)
		dt->uscale = params[i_u]->get_first_value<float>();

	int i_v = find_param("vscale", params);
	if (i_v >= 0)
		dt->vscale = params[i_v]->get_first_value<float>();
}

//
// execute_Texture
//
void PBRTParser::execute_Texture() {
	// TODO: repeat information is lost. One should save texture_info instead
	this->advance();

	std::shared_ptr<DeclaredTexture> dt(new DeclaredTexture);
	ygl::texture *txt = new ygl::texture();
	txt->name = get_unique_id(CounterID::texture);
	txt->path = textureSavePath + "/" + txt->name + ".png";
	dt->txt = txt;

	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_exception("Expected texture name string.");
	std::string textureName = this->current_token().value;
	
	auto it = gState.nameToTexture.find(textureName);
	if (it != gState.nameToTexture.end())
		gState.nameToTexture.erase(it);

	this->advance();
	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_exception("Expected texture type string.");
	std::string textureType = check_synonyms(this->current_token().value);

	if (textureType != "spectrum" && textureType != "rgb" && textureType != "float")
		throw_syntax_exception("Unsupported texture base type: " + textureType);
	
	this->advance();
	if (this->current_token().type != LexemeType::STRING)
		throw_syntax_exception("Expected texture class string.");
	std::string textureClass = this->current_token().value;
	this->advance();

	if (textureClass == "imagemap") {
		this->parse_imagemap_texture(dt);
	}
	else if (textureClass == "checkerboard") {
		this->parse_checkerboard_texture(dt);
	}
	else if (textureClass == "constant") {
		this->parse_constant_texture(dt);
	}
	else if (textureClass == "scale") {
		this->parse_scale_texture(dt);
	}
	else {
		throw_syntax_exception("Texture class not supported: " + textureClass);
	}

	gState.nameToTexture.insert(std::make_pair(textureName, dt));
}

// ==========================================================================================
//                                    AUXILIARY FUNCTIONS
// ==========================================================================================

//
// make_constant_image
//
ygl::image4b make_constant_image(float v) {
	auto  x = ygl::image4b(1, 1);
	auto b = ygl::float_to_byte(v);
	x.at(0, 0) = { b, b, b, 255 };
	return x;
}

//
// make_constant_image
//
ygl::image4b make_constant_image(ygl::vec3f v) {
	auto  x = ygl::image4b(1, 1);
	x.at(0, 0) = ygl::float_to_byte({ v.x, v.y, v.z, 1 });
	return x;
}

//
// find_param
// search for a parameter by name in a vector of parsed parameters.
// Returns the index of the searched parameter in the vector if found, -1 otherwise.
//
int find_param(std::string name, std::vector<std::shared_ptr<PBRTParameter>> &vec) {
	int count = 0;
	for (std::shared_ptr<PBRTParameter> &P : vec) {
		if (P->name == name)
			return count;
		else
			count++;
	}
	return -1;
}

//
// my_compute_normals
// because pbrt computes it differently
// TODO: must be removed in future.
//
void my_compute_normals(const std::vector<ygl::vec3i>& triangles,
	const std::vector<ygl::vec3f>& pos, std::vector<ygl::vec3f>& norm, bool weighted) {
	norm.resize(pos.size());
	for (auto& n : norm) n = ygl::zero3f;
	for (auto& t : triangles) {
		auto n = cross(pos[t.y] - pos[t.z], pos[t.x] - pos[t.z]); // it is different here
		if (!weighted) n = normalize(n);
		for (auto vid : t) norm[vid] += n;
	}
	for (auto& n : norm) n = normalize(n);
}
