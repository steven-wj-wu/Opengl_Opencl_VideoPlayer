__kernel void pixel_extract(__global char *mem_source, __global char *mem_dest, int width)
{
	
	 int i = 3 * get_global_id(0); //width RGB RGB
	 int j = get_global_id(1); //height
	 unsigned long ptr = j * width * 3 + i;     
	 
	mem_dest[ptr] = mem_source[ptr+2]; //RB
	mem_dest[ptr + 1] =mem_source[ptr+1]; //GG
	mem_dest[ptr + 2] = mem_source[ptr]; //BR
	
	
}