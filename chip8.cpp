#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <SDL2/SDL.h>
#include <string>
#include <fstream>
#include <algorithm>
#include <cstdlib>

using namespace std;

#define PIXEL_SIZE (10)

void beep(){
	system("( speaker-test -t sine -f 1000 > /dev/null )& pid=$! ; sleep 0.1s ; kill -9 $pid");
}

SDL_Window *window;
SDL_Renderer* renderer;

unsigned char chip8_fontset[80] =
{ 
	0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
	0x20, 0x60, 0x20, 0x20, 0x70, // 1
	0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
	0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
	0x90, 0x90, 0xF0, 0x10, 0x10, // 4
	0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
	0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
	0xF0, 0x10, 0x20, 0x40, 0x40, // 7
	0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
	0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
	0xF0, 0x90, 0xF0, 0x90, 0x90, // A
	0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
	0xF0, 0x80, 0x80, 0x80, 0xF0, // C
	0xE0, 0x90, 0x90, 0x90, 0xE0, // D
	0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
	0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

class chip8{
private:
	unsigned char memory[4096];
	unsigned char v[16];
	unsigned short stack[16];
	unsigned char delay_timer;
	unsigned char sound_timer;
	unsigned short I;
	unsigned short pc;
	unsigned short sp;
	bool keys[16];
	unsigned char pressed_key;
public:
	bool gfx[64][32];
	bool beep;
	chip8(const vector<unsigned char> &rom);
	chip8(const chip8 &) = delete;
	chip8(chip8 &&) = delete;
	chip8 &operator=(const chip8 &) = delete;
	chip8 &operator=(chip8 &&) = delete;
	~chip8() = default;
	void run(const vector<unsigned char> &rom);
	void cycle();
	void key_up(const unsigned char &hex);
	void key_down(const unsigned char &hex);
	void key_press(const unsigned char &hex);
};

chip8::chip8(const vector<unsigned char> &rom):memory(),v(),stack(),delay_timer(),sound_timer(),gfx(),I(),pc(0x200),sp(),keys(),pressed_key(),beep(){
	copy_n(chip8_fontset,80,memory);
	copy(rom.begin(),rom.end(),&memory[0x200]);
}

void chip8::cycle(){
	
	unsigned short ins = memory[pc];
	ins = ins << 8;
	ins = ins | memory[pc+1];
	cout<<hex<<ins<<endl;
	
	switch(ins & 0xF000){
		case 0x0000:
			{
				if(ins == 0x00E0){// Clear screen (DONE!)
					for(size_t i=0;i<64;i++)
						for(size_t j=0;j<32;j++)
							gfx[i][j]=false;
					pc += 2;
				}else if(ins == 0x00EE){ // Return from subroutine (DONE!)
					sp--;
					pc=stack[sp];
					stack[sp]=0;
					pc+=2;
				}else{
					// This instruction is not used in many ROMs!
					//pc+=2;
				}
			}
			break;
		case 0x1000: // Jumps to address NNN (0x1NNN) (DONE!)
			{
				unsigned short addr=ins & 0x0FFF;
				pc = addr;
			}
			break;
		case 0x2000: // Calls subroutine at NNN (0x2NNN) (DONE!)
			{
				unsigned short addr = ins & 0x0FFF;
				stack[sp]=pc;
				sp++;
				pc=addr;
			}
			break;
		case 0x3000: // Skips the next instruction if VX equals NN (0x3XNN) (DONE!)
			{
				size_t v_reg_index = (ins & 0x0F00) >> 8;
				unsigned char num = ins & 0x00FF;
				if(v[v_reg_index]==num)
					pc += 4;
				else
					pc += 2;
			}
			break;
		case 0x4000: // Skips the next instruction if VX doesn't equal NN (0x4XNN) (DONE!)
			{
				size_t v_reg_index = (ins & 0x0F00) >> 8;
				unsigned char num = ins & 0x00FF;
				if(v[v_reg_index]!=num)
					pc += 4;
				else
					pc += 2;
			}
			break;
		case 0x5000: // Skips the next instruction if VX equals VY (0x5XY0) (DONE!)
			{
				if((ins & 0x000F) == 0x0000){
					size_t vx_reg_index = (ins & 0x0F00) >> 8;
					size_t vy_reg_index = (ins & 0x00F0) >> 4;
					if(v[vx_reg_index]==v[vy_reg_index])
						pc += 4;
					else
						pc += 2;
				}
			}
			break;
		case 0x6000: // Sets VX to NN (0x6XNN) (DONE!)
			{
				size_t vx_reg_index = (ins & 0x0F00) >> 8;
				unsigned char num = ins & 0x00FF;
				v[vx_reg_index]=num;
				pc += 2;
			}
			break;
		case 0x7000: // Add NN to VX (0x7XNN) (DONE!)
			{
				size_t vx_reg_index = (ins & 0x0F00) >> 8;
				unsigned char num = ins & 0x00FF;
				v[vx_reg_index]+=num;
				pc += 2;
			}
			break;
		case 0x8000: // (0x8XY?)
			{
				size_t vx_reg_index = (ins & 0x0F00) >> 8;
				size_t vy_reg_index = (ins & 0x00F0) >> 4;
				switch(ins & 0x000F){
					case 0x0000:
						// Sets VX to the value of VY (DONE!)
						{
							v[vx_reg_index]=v[vy_reg_index];
							pc += 2;
						}
						break;
					case 0x0001:
						// Sets VX to VX | VY (DONE!)
						{
							v[vx_reg_index] |= v[vy_reg_index];
							pc += 2;
						}
						break;
					case 0x0002:
						// Sets VX to VX & VY (DONE!)
						{
							v[vx_reg_index] &= v[vy_reg_index];
							pc += 2;
						}
						break;
					case 0x0003:
						// Sets VX to VX ^ VY (DONE!)
						{
							v[vx_reg_index] ^= v[vy_reg_index];
							pc += 2;
						}
						break;
					case 0x0004:
						// Adds VY to VX. VF is set to 1 when there's a carry, and to 0 when there isn't. (DONE!)
						{
							unsigned int intsum=(unsigned int)v[vx_reg_index]+(unsigned int)v[vy_reg_index];
							bool carry = (intsum  & 0xFFFFFF00);
							if(carry)
								v[0xF]=1;
							else
								v[0xF]=0;
							v[vx_reg_index] = (intsum  & 0x000000FF);
							pc += 2;
							// MAY NOT WORK PROPERLY!
						}
						break;
					case 0x0005:
						// VY is subtracted from VX. VF is set to 0 when there's a borrow, and 1 when there isn't. (DONE!)
						{
							int intsub=(int)v[vx_reg_index]-(int)v[vy_reg_index];
							bool carry = intsub<0;
							if(carry)
								v[0xF]=1;
							else
								v[0xF]=0;
							v[vx_reg_index] = ((unsigned int)intsub  & 0x000000FF);
							pc += 2;
							// MAY NOT WORK PROPERLY!
						}
						break;
					case 0x0006:
						// Shifts VX right by one and store in VY. VF is set to the value of the least significant bit of VX before the shift. (DONE!)
						{
							v[0xF]=v[vx_reg_index] & 1;
							v[vy_reg_index] = v[vx_reg_index]>>1;
							pc += 2;
						}
						break;
					case 0x0007:
						// Sets VX to VY minus VX. VF is set to 0 when there's a borrow, and 1 when there isn't. (DONE!)
						{
							int intsub=(int)v[vy_reg_index]-(int)v[vx_reg_index];
							bool carry = intsub<0;
							if(carry)
								v[0xF]=1;
							else
								v[0xF]=0;
							v[vx_reg_index] = ((unsigned int)intsub  & 0x000000FF);
							pc += 2;
							// MAY NOT WORK PROPERLY!
						}
						break;
					case 0x000E:
						// Shifts VX left by one and store in VY. VF is set to the value of the most significant bit of VX before the shift. (DONE!)
						{
							v[0xF]=v[vx_reg_index] & 1;
							v[vy_reg_index] = v[vx_reg_index]<<1;
							pc += 2;
						}
						break;
					default:
						break;
						
				}
			}
			break;
		case 0x9000: // Skips the next instruction if VX doesn't equal VY (0x9XY0) (DONE!)
			{
				if((ins & 0x000F) == 0x0000){
					size_t vx_reg_index = (ins & 0x0F00) >> 8;
					size_t vy_reg_index = (ins & 0x00F0) >> 4;
					if(v[vx_reg_index]!=v[vy_reg_index])
						pc += 4;
					else
						pc += 2;
				}
			}
			break;
		case 0xA000: // Sets I to NNN (0xANNN) (DONE!)
			{
				unsigned short num = ins & 0x0FFF;
				I = num;
				pc += 2;
			}
			break;
		case 0xB000: // Jumps to the address NNN plus V0 (0xBNNN) (DONE!)
			{
				unsigned short num=ins & 0x0FFF;
				pc = num + v[0x0];
			}
			break;
		case 0xC000: // Sets VX to the result of a bitwise and operation on a random number and NN (0xCXNN) (DONE!)
			{
				size_t vx_reg_index = (ins & 0x0F00) >> 8;
				unsigned char num = ins & 0x00FF;
				v[vx_reg_index] = rand() & num;
				pc += 2;
			}
			break;
		case 0xD000: // Draws a sprite at coordinate (VX, VY) that has a width of 8 pixels and a height of N pixels. Each row of 8 pixels is read as bit-coded starting from memory location I; I value doesn’t change after the execution of this instruction. As described above, VF is set to 1 if any screen pixels are flipped from set to unset when the sprite is drawn, and to 0 if that doesn’t happen. (0xDXYN) (DONE!)
			{
				v[0xF]=0;
				size_t vx_reg_index = (ins & 0x0F00) >> 8;
				size_t vy_reg_index = (ins & 0x00F0) >> 4;
				size_t x=v[vx_reg_index];
				size_t y=v[vy_reg_index];
				size_t height = ins & 0x000F;
				for(size_t jc=y;jc<y+height;jc++)
					for(size_t ic=x;ic<x+8;ic++){
						bool state=memory[I+jc-y] & (1<<7>>(ic-x));
						if(state){
							if(gfx[ic][jc]){
								gfx[ic][jc]=false;
								v[0xF]=1;
							}
							else{
								gfx[ic][jc]=true;
							}
						}
					}
				pc += 2;
				this_thread::sleep_for(chrono::milliseconds(20));
			}
			break;
		case 0xE000:
			{
				size_t vx_reg_index = (ins & 0x0F00) >> 8;
				if((ins & 0x00FF) == 0x009E){
					// Skip the following instruction if the key corresponding to the hex value currently stored in register VX is pressed (0xEX9E) (DONE)
					if(keys[v[vx_reg_index]])
						pc += 4;
					else
						pc += 2;
				}
				else if((ins & 0x00FF) == 0x00A1){
					// Skip the following instruction if the key corresponding to the hex value currently stored in register VX is not pressed (0xEXA1) (DONE)
					if(!keys[v[vx_reg_index]])
						pc += 4;
					else
						pc += 2;
				}
			}
			break;
		case 0xF000:
			{
				size_t vx_reg_index = (ins & 0x0F00) >> 8;
				unsigned short command = ins & 0x00FF;
				if(command == 0x0007){
					// Store the current value of the delay timer in register VX (DONE!)
					v[vx_reg_index] = delay_timer;
					pc += 2;
				}
				else if(command == 0x000A){
					// Wait for a keypress and store the result in register VX (DONE!)
					if(pressed_key){
						v[vx_reg_index]=pressed_key;
						pressed_key=0;
						pc += 2;
					}
				}
				else if(command == 0x0015){
					// Set the delay timer to the value of register VX (DONE!)
					delay_timer = v[vx_reg_index];
					pc += 2;
				}
				else if(command == 0x0018){
					// Set the sound timer to the value of register VX (DONE!)
					sound_timer = v[vx_reg_index];
					pc += 2;
				}
				else if(command == 0x001E){
					// Add the value stored in register VX to register I (DONE!)
					I += v[vx_reg_index];
					pc += 2;
				}
				else if(command == 0x0029){
					// Sets I to the location of the sprite for the character in VX. Characters 0-F (in hexadecimal) are represented by a 4x5 font. (DONE!)
					I = v[vx_reg_index]*0x5;
					pc += 2;
				}
				else if(command == 0x0033){
					// Store the binary-coded decimal equivalent of the value stored in register VX at addresses I, I+1, and I+2 (Done!)
					unsigned char val = v[vx_reg_index];
					memory[I] = val/100 % 10;
					memory[I+1] = val/10 % 10;
					memory[I+2] = val % 10;
					pc += 2;
				}
				else if(command == 0x0055){
					// Store the values of registers V0 to VX inclusive in memory starting at address I
					// I is set to I + X + 1 after operation (DONE!)
					for(size_t ind=0;ind<=vx_reg_index;ind++){
						memory[I+ind] = v[ind];
					}
					I=I+vx_reg_index+1;
					pc += 2;
				}
				else if(command == 0x0065){
					// Fill registers V0 to VX inclusive with the values stored in memory starting at address I
					// I is set to I + X + 1 after operation (DONE!)
					for(size_t ind=0;ind<=vx_reg_index;ind++){
						v[ind] = memory[I+ind];
					}
					I=I+vx_reg_index+1;
					pc += 2;
				}
			}
			break;
		default:
			break;
	}
	
	if(delay_timer>0)
		delay_timer--;
	if(sound_timer>0){
		if(sound_timer==1)
			beep=true;
		sound_timer--;
	}
}

void chip8::key_down(const unsigned char &hex){
	keys[hex]=true;
}
void chip8::key_up(const unsigned char &hex){
	keys[hex]=false;
}
void chip8::key_press(const unsigned char &hex){
	pressed_key=hex;
}

void update_display(bool gfx[64][32]){
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255 );
    SDL_RenderClear(renderer);
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255 );
	for(size_t j=0;j<32;j++){
		for(size_t i=0;i<64;i++){
			if(gfx[i][j]){
				SDL_Rect r;
				r.x = PIXEL_SIZE*i;
				r.y = PIXEL_SIZE*j;
				r.w = PIXEL_SIZE;
				r.h = PIXEL_SIZE;
				SDL_RenderFillRect(renderer,&r);
			}
		}
	}
	SDL_RenderPresent(renderer);
}

vector<unsigned char> read_rom(const string &path){
	ifstream rom_file(path,ios::in | ios::binary);
	vector<unsigned char> rom;
	while(!rom_file.eof()){
		rom.push_back((unsigned char)rom_file.get());
	}
	return rom;
}

int main(int argc, char *argv[]){
	window = SDL_CreateWindow("CHIP-8 Emulator",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,PIXEL_SIZE*64,PIXEL_SIZE*32,0);
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	
	if(argc<=1){
		cout<<"Please specify a ROM! (E.g. ./chip8 ROMS/TANK)"<<endl;
		return 0;
	}
	
	chip8 chip(read_rom(argv[1]));
	
	while(true){
		SDL_Event event;
		while(SDL_PollEvent(&event)){
			switch(event.type){
				case SDL_QUIT:
					exit(0);
					break;
				case SDL_KEYDOWN:
					switch(event.key.keysym.sym){
						case SDLK_1:
							chip.key_down(0x1);
							break;
						case SDLK_2:
							chip.key_down(0x2);
							break;
						case SDLK_3:
							chip.key_down(0x3);
							break;
						case SDLK_4:
							chip.key_down(0xC);
							break;
						case SDLK_q:
							chip.key_down(0x4);
							break;
						case SDLK_w:
							chip.key_down(0x5);
							break;
						case SDLK_e:
							chip.key_down(0x6);
							break;
						case SDLK_r:
							chip.key_down(0xD);
							break;
						case SDLK_a:
							chip.key_down(0x7);
							break;
						case SDLK_s:
							chip.key_down(0x8);
							break;
						case SDLK_d:
							chip.key_down(0x9);
							break;
						case SDLK_f:
							chip.key_down(0xE);
							break;
						case SDLK_z:
							chip.key_down(0xA);
							break;
						case SDLK_x:
							chip.key_down(0x0);
							break;
						case SDLK_c:
							chip.key_down(0xB);
							break;
						case SDLK_v:
							chip.key_down(0xF);
							break;
						default:
							break;
					}
					break;
				case SDL_KEYUP:
					switch(event.key.keysym.sym){
						case SDLK_1:
							chip.key_up(0x1);
							chip.key_press(0x1);
							break;
						case SDLK_2:
							chip.key_up(0x2);
							chip.key_press(0x2);
							break;
						case SDLK_3:
							chip.key_up(0x3);
							chip.key_press(0x3);
							break;
						case SDLK_4:
							chip.key_up(0xC);
							chip.key_press(0xC);
							break;
						case SDLK_q:
							chip.key_up(0x4);
							chip.key_press(0x4);
							break;
						case SDLK_w:
							chip.key_up(0x5);
							chip.key_press(0x5);
							break;
						case SDLK_e:
							chip.key_up(0x6);
							chip.key_press(0x6);
							break;
						case SDLK_r:
							chip.key_up(0xD);
							chip.key_press(0xD);
							break;
						case SDLK_a:
							chip.key_up(0x7);
							chip.key_press(0x7);
							break;
						case SDLK_s:
							chip.key_up(0x8);
							chip.key_press(0x8);
							break;
						case SDLK_d:
							chip.key_up(0x9);
							chip.key_press(0x9);
							break;
						case SDLK_f:
							chip.key_up(0xE);
							chip.key_press(0xE);
							break;
						case SDLK_z:
							chip.key_up(0xA);
							chip.key_press(0xA);
							break;
						case SDLK_x:
							chip.key_up(0x0);
							chip.key_press(0x0);
							break;
						case SDLK_c:
							chip.key_up(0xB);
							chip.key_press(0xB);
							break;
						case SDLK_v:
							chip.key_up(0xF);
							chip.key_press(0xF);
							break;
						default:
							break;
					}
					break;
			}
		}
		chip.cycle();
		if(chip.beep){
			beep();
			chip.beep=false;
		}
		update_display(chip.gfx);
	}
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
