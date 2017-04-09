### Rebellion  
Chris Pilkington  
Copyright (C) 2017  
<http://chris.iluo.net/> 

If you find this project helpful, please consider making a donation. 

[<img alt="Make a donation via Pledgie" src="http://www.pledgie.com/campaigns/17973.png?skin_name=chrome" border="0" />][1]  

### License

This program is free software; you can redistribute it and/or  
modify it under the terms of the GNU General Public License  
as published by the Free Software Foundation; either version 3  
of the License, or (at your option) any later version. 

This program is distributed in the hope that it will be useful,  
but WITHOUT ANY WARRANTY; without even the implied warranty of  
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  
GNU General Public License for more details. 

You should have received a copy of the GNU General Public License  
along with this program; if not, write to the Free Software  
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.  

### Description  

Project for the game "Rebellion"  

### Usage

The cmake project can be run like so:  
cmake .  
make  
./rebellion  

### Getting a copy of the project on Linux (Fedora 14 used)

Pull this project:  
yum install git  
OR  
sudo apt-get install git-core  
git clone git@github.com:pilkch/rebellion.git  
OR  
git clone https://github.com/pilkch/rebellion.git  

### Building on Linux (Fedora 14 used)

Rebellion requires the [spitfire library][2], it should be checked out at the same level as rebellion:  
source/  
- library/  
- rebellion/  

Various other libraries are also required such as [SDL][3], [ODE][4].

### Credit

Breathe and Spitfire are created by me, Christopher Pilkington.   
For the parts that are not mine, I try to keep an up to date list of any third party libraries that I use.   
I only use libraries that are license under either the GPL, LGPL or similar and am eternally grateful for the high quality ease of use and generosity of the open source community.  

Open Dynamics Engine  
Copyright (C) 2001-2003 Russell L. Smith  
All rights reserved  
<http://www.q12.org/>  

SDL - Simple DirectMedia Layer  
Copyright (C) 1997-2006 Sam Lantinga  
Sam Lantinga  
<http://www.libsdl.org/>  

FreeType Copyright 1996-2001, 2006 by  
David Turner, Robert Wilhelm, and Werner Lemberg  
<http://www.freetype.org/> 

3DS File Loader  
genjix@gmail.com  
<http://sourceforge.net/projects/scene3ds>  
<http://www.gamedev.net/community/forums/topic.asp?topic_id=313126> 

MD5 RFC 1321 compliant MD5 implementation  
Copyright (C) 2001-2003 Christophe Devine 

Memory manager & tracking software  
Paul Nettle  
Copyright 2000, Fluid Studios  
All rights reserved  
<http://www.FluidStudios.com>

 [1]: http://www.pledgie.com/campaigns/17973
 [2]: https://github.com/pilkch/library/
 [3]: http://www.libsdl.org/
 [4]: http://www.q12.org/
