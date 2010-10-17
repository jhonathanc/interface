// Module related
#include <smem.h>
#include <smod.h>
#include <loadfile.h>

// SIF RPC related
#include <sifrpc.h>

// sbv patches
#include <sbv_patches.h>

// IOP rebooting
#include <iopcontrol.h>

// Library initialization
#include <libmc.h>
#include <libmtap.h>
#include <libpad.h>
#include <libpwroff.h>
#include <fileXio_rpc.h>
#include <audsrv.h>

#include <gs_psm.h>
#include <draw.h>
#include <packet.h>
#include <dma.h>
#include <graph.h>

// Standard headers
#include <stdio.h>
#include <string.h>

#include <zlib.h>


#include "init.h"

#include "tar.h"
#include "gzip.h"

static int __dev9_initialized = 0;

void init_load_erom(void)
{

	int ret = 0;

	if (SifLoadStartModule("rom0:ADDDRV", 0, NULL, &ret) < 0)
	{
#ifdef DEBUG
		printf("Failed to load module: ADDDRV\n");
#endif
	}

	if (ret)
	{
#ifdef DEBUG
		printf("Failed to start module: ADDDRV\n");
#endif
	}

	if (SifLoadModuleEncrypted("rom1:EROMDRV", 0, NULL) < 0)
	{
#ifdef DEBUG
		printf("Failed to load encrypted module: EROMDRV\n");
#endif
	}

}

void list_loaded_modules(void)
{
	char search_name[60];
	smod_mod_info_t	mod_t;

	smod_get_next_mod(NULL,&mod_t);

	smem_read(mod_t.name, search_name, sizeof search_name);

	printf("Module %d is %s\n", mod_t.id, search_name);

	while(smod_get_next_mod(&mod_t,&mod_t))
	{
		smem_read(mod_t.name, search_name, sizeof search_name);
		printf("Module %d is %s\n", mod_t.id, search_name);
	}

}

int init_load_bios(module_t *modules, int num)
{

	int i = 0;
	int ret = 0;

	smod_mod_info_t	mod_t;

	for (i = 0; i < num; i++)
	{
		if (!smod_get_mod_by_name(modules[i].name, &mod_t))
		{
			if ((SifLoadStartModule(modules[i].module, 0, NULL,&modules[i].result) < 0))
			{
#ifdef DEBUG
				printf("Failed to load module: %s\n", modules[i].module);
#endif
				ret = -1;
			}

			if (modules[i].result)
			{
#ifdef DEBUG
				printf("Failed to start module: %s\n", modules[i].module);
#endif
				ret = -2;
			}
		}
		else
		{
#ifdef DEBUG
			printf("Possible module conflict\n");
#endif
			ret = -3;
		}
	}

	return ret;

}

int init_load_irx(const char *dir, module_t *modules, int num)
{

	char path[256];
	int i,size,ret = 0;

	char *gz = NULL;
	char *tar;

	char *module;
	int module_size;

	smod_mod_info_t mod_t;

	strcpy(path,dir);
	strcat(path,"/modules.tgz");

	gz = gzip_load_file(path,&size);

	if (gz == NULL)
	{
		return -1;
	}

	size = gzip_get_size(gz,size);
	tar = malloc(size);

	if ((ret = gzip_uncompress(gz,tar)) != Z_OK)
	{
		free(gz);
		free(tar);
		return -1;
	}

	free(gz);

	for(i = 0; i < num; i++)
	{
		if(!smod_get_mod_by_name(modules[i].name, &mod_t))
		{
			if (get_file_from_tar(tar,size,modules[i].module, &module, &module_size) < 0)
			{
#ifdef DEBUG
				printf("Failed to find module: %s\n", modules[i].module);
#endif
				free(tar);
				modules[i].result = -1;
				return -1;
			}

			if (SifExecModuleBuffer(module, module_size,modules[i].arglen, modules[i].args, &modules[i].result) < 0)
			{
#ifdef DEBUG
				printf("Failed to load module: %s\n", modules[i].module);
#endif
				free(tar);
				modules[i].result = -2;
				return -2;
			}

			if (modules[i].result)
			{
				printf("Failed to start module: %s\n", modules[i].module);
				free(tar);
				return -3;
			}

		}
		else
		{
#ifdef DEBUG
			printf("Possible module conflict\n");
#endif
			free(tar);
			return -4;
		}
	}

	free(tar);

	return 0;
}

void init_sbv_patches()
{
	sbv_patch_enable_lmb();
	sbv_patch_disable_prefix_check();
}

void init_iop(void)
{

	SifInitRpc(0);

	init_sbv_patches();

	SifIopReboot("rom0:EELOADCNF");

#ifndef V0_PS2
	init_x_bios_modules();
#else
	init_bios_modules();
#endif

}

void init_x_bios_modules()
{

	module_t basic_modules[6] =
	{
		{ "sio2man"   , "rom0:XSIO2MAN", NULL, 0, 0 },
		{ "mcman_cex" , "rom0:XMCMAN"  , NULL, 0, 0 },
		{ "mcserv"    , "rom0:XMCSERV" , NULL, 0, 0 },
		{ "mtapman"   , "rom0:XMTAPMAN", NULL, 0, 0 },
		{ "padman"    , "rom0:XPADMAN" , NULL, 0, 0 },
		{ "noname"    , "rom0:XCDVDMAN", NULL, 0, 0 }
	};

	init_load_bios(basic_modules,6);

	// Init various libraries
	mcInit(MC_TYPE_XMC);

	mtapInit();
	padInit(0);

	mtapPortOpen(0);
	mtapPortOpen(1);

}

void init_bios_modules()
{

	module_t basic_modules[7] =
	{
		{ "sio2man"        , "rom0:SIO2MAN", NULL, 0, 0 },
		{ "mcman"          , "rom0:MCMAN"  , NULL, 0, 0 },
		{ "mcserv"         , "rom0:MCSERV" , NULL, 0, 0 },
		{ "padman"         , "rom0:PADMAN" , NULL, 0, 0 },
		{ "noname"         , "rom0:CDVDMAN", NULL, 0, 0 },
		{ "IO/File_Manager", "rom0:IOMAN"  , NULL, 0, 0 },
		{ "FILEIO_service" , "rom0:FILEIO" , NULL, 0, 0 }
	};

	init_load_bios(basic_modules,7);

	// Init various libraries
	mcInit(MC_TYPE_MC);
	padInit(0);

}

void init_x_irx_modules(const char *dir)
{

	module_t basic_modules[2] =
	{
		{
			"IOX/File_Manager",
			"iomanX.irx",
			NULL,
			0,
			0
		},
		{
			"IOX/File_Manager_Rpc",
			"fileXio.irx",
			NULL,
			0,
			0
		},

	};

	init_load_irx(dir,basic_modules,2);

	if (!basic_modules[1].result)
	{
		fileXioInit();
	}
	else
	{
		fioInit();
	}

}

void init_dev9_irx_modules(const char *dir)
{

	module_t dev9_modules[2] =
	{
		{
			"Poweroff_Handler",
			"poweroff.irx",
			NULL,
			0,
			0
		},
		{
			"dev9_driver",
			"ps2dev9.irx",
			NULL,
			0,
			0
		}
	};

	init_load_irx(dir,dev9_modules,2);

	// Return if the poweroff module failed
	if (dev9_modules[0].result)
	{
		return;
	}

	//
	if (!dev9_modules[1].result)
	{
		__dev9_initialized = 1;
	}
	poweroffInit();

}

void init_usb_modules(const char *dir)
{

	module_t usb_modules[2] =
	{
		{
			"usbd",
			"usbd.irx",
			NULL,
			0,
			0
		},
		{
			"usb_mass",
			"usbhdfsd.irx",
			NULL,
			0,
			0
		}
	};

	init_load_irx(dir,usb_modules,2);

}

void init_hdd_modules(const char *dir)
{

	static char hddarg[] = "-o" "\0" "4" "\0" "-n" "\0" "20";
	static char pfsarg[] = "-m" "\0" "4" "\0" "-o" "\0" "10" "\0" "-n" "\0" "40";

	module_t hdd_modules[3] =
	{
		{
			"atad",
			"ps2atad.irx",
			NULL,
			0,
			0
		},
		{
			"hdd_driver",
			"ps2hdd.irx",
			hddarg,
			sizeof(hddarg),
			0
		},
		{
			"pfs_driver",
			"ps2fs.irx",
			pfsarg,
			sizeof(pfsarg),
			0
		}
	};

	if (!__dev9_initialized)
	{
		return;
	}

	init_load_irx(dir,hdd_modules,3);

}

void init_sound_modules(const char *dir)
{

	module_t libsd_module =
	{
		"libsd",
		"rom0:LIBSD",
		NULL,
		0,
		0
	};

	module_t audsrv_module =
	{
		"audsrv",
		"audsrv.irx",
		NULL,
		0,
		0
	};

	init_load_bios(&libsd_module,1);
	init_load_irx(dir,&audsrv_module,1);

	audsrv_init();
}
