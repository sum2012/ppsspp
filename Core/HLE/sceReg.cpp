
#include <cmath>
#include <vector>
#include <map>
#include <algorithm>
#include <string>


#include "Core/HLE/HLE.h"



#include "file/ini_file.h"

typedef unsigned int u32;

typedef struct {
	char name[64];
	int cid;
	int kid;
	int type;
	int size;
	char value[32];
}SCEREG_OBJ;

static std::map<int, SCEREG_OBJ> scereg_objects;
static IniFile iniFile;

static std::string registry_name;
static int scereg_id;
static int scereg_load;
static int scereg_open;

/**************************************************************/

void __RegInit()
{
	std::string tmp = "flash0:/registry.txt";
	pspFileSystem.GetHostPath(tmp, registry_name);

	scereg_id = 1;
	scereg_load = 0;
	scereg_open = 0;
}

void __RegShutdown()
{
	scereg_objects.clear();
	scereg_open = 0;
}


void __RegDoState(PointerWrap &p)
{
	p.Do(scereg_open);
	if(scereg_open){
		p.Do(scereg_objects);
	}

	p.DoMarker("sceReg");

	if(scereg_open){
		iniFile.Load(registry_name.c_str());
		scereg_load = 1;
	}
}


/**************************************************************/

/* Key types */
enum RegKeyTypes
{
	REG_TYPE_NONE= 0,
    REG_TYPE_DIR = 1,    /** Key is a  directory */
    REG_TYPE_INT = 2,    /** Key is an integer (4 bytes) */
    REG_TYPE_STR = 3,    /** Key is a  string */
    REG_TYPE_BIN = 4,    /** Key is a  binary string */
};

SCEREG_OBJ *getRegObj(int key)
{
	std::map<int, SCEREG_OBJ>::iterator iter;
	SCEREG_OBJ *obj;

	iter = scereg_objects.find(key);
	if(iter==scereg_objects.end())
		return NULL;

	obj = &iter->second;
	return obj;
}



int findCategory(const char *name)
{
	SCEREG_OBJ obj;

	if(iniFile.HasSection(name)){
		strcpy(obj.name, name);
		obj.cid = scereg_id;
		obj.kid = 0;
		scereg_objects[obj.cid] = obj;
		scereg_id += 1;

		return obj.cid;
	}

	return -1;
}

bool getKeyValue(const char *c_name, const char *key_name, std::string *value = NULL)
{
	std::vector<std::string> lines;
	std::vector<std::string>::const_iterator iter;

	iniFile.GetLines(c_name, lines);

	for (iter=lines.begin(); iter!=lines.end(); ++iter){
		std::string line = StripSpaces(*iter);

		int pos = (int)line.find('=');
		if(pos!=(int)std::string::npos){
			if(value)
				*value = StripSpaces(line.substr(pos+1));
			line  = StripSpaces(line.substr(0, pos-1));
		}
		if(!strcmp(line.c_str(), key_name)){
			return true;
		}
	}

	return false;
}

SCEREG_OBJ *parseKey(int hd, const char *k_name)
{
	SCEREG_OBJ *obj_c;
	SCEREG_OBJ obj_k;
	std::string value;
	char d_name[64];
	bool retv;
	int i;

	obj_c = getRegObj(hd);
	if(obj_c==NULL)
		return NULL;


	retv = getKeyValue(obj_c->name, k_name, &value);
	if(retv==false){
		sprintf(d_name, "<%s>", k_name);
		retv = getKeyValue(obj_c->name, d_name);
		if(retv==false)
			return NULL;

		obj_k.type = REG_TYPE_DIR;
		obj_k.size = 0;
	}else{
		if(value[0]=='"'){
			obj_k.type = REG_TYPE_STR;
			obj_k.size = value.size()-1;
			for(i=0; i<obj_k.size-1; i++)
				obj_k.value[i] = value[i+1];
			obj_k.value[i] = 0;
		}else{
			obj_k.type = REG_TYPE_INT;
			obj_k.size = 4;
			int i_value = atoi(value.c_str());
			*(int*)(obj_k.value) = i_value;
		}

	}

	strcpy(obj_k.name, k_name);
	obj_k.cid = hd;
	obj_k.kid = scereg_id;
	scereg_id += 1;
	scereg_objects[obj_k.kid] = obj_k;

	return &scereg_objects[obj_k.kid];
}


/**************************************************************/

int sceRegOpenRegistry(u32 reg_ptr, int mode, u32 h_ptr)
{
	INFO_LOG(HLE, "sceRegOpenRegistry()");

	if(scereg_load==0){
		if (!iniFile.Load(registry_name.c_str())) {
			ERROR_LOG(HLE, "sceRegOpenRegistry: Failed to read %s", registry_name.c_str());
			return -1;
		}
		scereg_load = 1;
	}

	printf("sceRegOpenRegistry: Open %s.\n", registry_name);
	scereg_open = 1;

	if (Memory::IsValidAddress(h_ptr)) {
		Memory::Write_U32(scereg_id, h_ptr);
		return 0;
	} else {
		return -2;
	}
}

int sceRegCloseRegistry(u32 h)
{
	INFO_LOG(HLE, "sceRegCloseRegistry()\n");

	scereg_open = 0;
	scereg_objects.clear();

	return 0;
}

int sceRegOpenCategory(u32 h, const char *name, int mode, u32 hd_ptr)
{
	int hd;

	INFO_LOG(HLE, "sceRegOpenCategory(%08x, %s, %08x, %08x)", h, name, mode, hd_ptr);

	hd = findCategory(name);
	if(hd==-1){
		ERROR_LOG(HLE, "  not found!");
		return -1;
	}

	if (Memory::IsValidAddress(hd_ptr)) {
		Memory::Write_U32(hd, hd_ptr);
		return 0;
	} else {
		return -2;
	}
}

int sceRegCloseCategory(int hd)
{
	std::map<int, SCEREG_OBJ>::iterator iter;
	SCEREG_OBJ *reg;

	INFO_LOG(HLE, "sceRegCloseCategory()");

	for(iter=scereg_objects.begin(); iter!=scereg_objects.end();){
		reg = &iter->second;
		if(reg->cid==hd){
			scereg_objects.erase(iter++);
		}else{
			++iter;
		}
	}

	return 0;
}

int sceRegGetKeysNum(int hd, u32 num_ptr)
{
	SCEREG_OBJ *reg;
	std::vector<std::string> lines;

	reg = getRegObj(hd);
	if(reg==NULL)
		return -1;

	iniFile.GetLines(reg->name, lines);

	if (Memory::IsValidAddress(num_ptr)) {
		Memory::Write_U32(lines.size(), num_ptr);
		return 0;
	} else {
		return -2;
	}
}


int sceRegGetKeys(int hd, u32 buf_ptr, int num)
{
	SCEREG_OBJ *reg;
	std::vector<std::string> lines;
	std::vector<std::string>::const_iterator iter;

	if (!Memory::IsValidAddress(buf_ptr))
		return -2;
	char *buf = (char*) Memory::GetPointer(buf_ptr);

	reg = getRegObj(hd);
	if(reg==NULL)
		return -1;

	iniFile.GetLines(reg->name, lines);

	for (iter=lines.begin(); iter!=lines.end(); ++iter){
		std::string line = StripSpaces(*iter);

		int pos = (int)line.find('=');
		if(pos!=(int)std::string::npos){
			line = StripSpaces(line.substr(0, pos-1));
		}else{
			pos = (int)line.find('>');
			line = line.substr(1, pos-1);
		}
		strncpy(buf, line.c_str(), 27);
		buf += 27;
	}

	return 0;
}

int sceRegGetKeyInfo(int hd, const char *k_name, u32 hk_ptr, u32 type_ptr, u32 size_ptr)
{
	SCEREG_OBJ *reg;

	INFO_LOG(HLE, "sceRegGetKeyInfo(%08x, %s, %08x, %08x, %08x)", hd, k_name, hk_ptr, type_ptr, size_ptr);

	if (!Memory::IsValidAddress(hk_ptr) || !Memory::IsValidAddress(type_ptr) || !Memory::IsValidAddress(size_ptr))
		return -2;

	reg = parseKey(hd, k_name);
	if(reg){
		Memory::Write_U32(reg->kid, hk_ptr);
		Memory::Write_U32(reg->type, type_ptr);
		Memory::Write_U32(reg->size, size_ptr);
		return 0;
	}else{
		ERROR_LOG(HLE, "    no such key!\n");
		return -1;
	}
}

int sceRegGetKeyValue(int hd, int hk, u32 val_ptr, int size)
{
	SCEREG_OBJ *reg;

	INFO_LOG(HLE, "sceRegGetKeyValue(%08x, %08x, %08x, %08x)", hd, hk, val_ptr, size);
	if (!Memory::IsValidAddress(val_ptr))
		return -2;

	reg = getRegObj(hk);
	if(reg){
		Memory::Memcpy(val_ptr, reg->value, size);
		return 0;
	}else{
		return -1;
	}
}


template <int func(u32, const char *, int, u32)> void WrapI_UCIU() {
	int retval = func(PARAM(0), Memory::GetCharPointer(PARAM(1)), PARAM(2), PARAM(3));
	RETURN(retval);
}


const HLEFunction sceReg[] = {
	{0x92E41280, WrapI_UIU<sceRegOpenRegistry>  , "sceRegOpenRegistry"},	
	{0xFA8A5739, WrapI_U<sceRegCloseRegistry>   , "sceRegCloseRegistry"},	
	{0x1D8A762E, WrapI_UCIU<sceRegOpenCategory> , "sceRegOpenCategory"},	
	{0x0CAE832B, WrapI_I<sceRegCloseCategory>   , "sceRegCloseCategory"},	
	{0xD4475AA8, WrapI_ICUUU<sceRegGetKeyInfo>  , "sceRegGetKeyInfo"},	
	{0x28A8E98A, WrapI_IIUI<sceRegGetKeyValue>  , "sceRegGetKeyValue"},	
	{0x2C0DB9DD, WrapI_IU<sceRegGetKeysNum>     , "sceRegGetKeysNum"},	
	{0x2D211135, WrapI_IUI<sceRegGetKeys>       , "sceRegGetKeys"},	
};

void Register_sceReg() {
	RegisterModule("sceReg", ARRAY_SIZE(sceReg), sceReg);
}

