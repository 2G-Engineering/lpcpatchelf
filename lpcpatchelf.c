/*
 * File       : lpcpatchelf.c
 *
 * Description: Utility to compute and patch the correct flash image
 *              checksum into LPC17xx (and other LPC microcontrollers)
 *
 * Author     : Nils Pipenbrinck <n.pipenbrinck@hilbert-space.de>
 * Copyright  : (C) Nils Pipenbrinck 2016
 * Website    : http://hilbert-space.de/?p=178
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <libelf.h>
#include <stdbool.h>
#include <getopt.h>

#define VERSION_MAJ 1
#define VERSION_MIN 0
#define NUMVECTORS  8

static uint32_t get_lpc_signature (const uint32_t *sig,
                                        int NumVectors,
                                        int CheckVector)
//////////////////////////////////////////////////////////////
{
    uint32_t cksum = 0;
    for (int i = 0; i < NumVectors; i++)
    {
        if (i != CheckVector)
            cksum += sig[i];
    }
    return 0-cksum;
}


bool check_arm (Elf *e)
///////////////////////
{
    Elf32_Ehdr * hdr = elf32_getehdr (e);
    if (!hdr)
    {
        fprintf (stderr, "elf32_getehdr failed %s\n", elf_errmsg(elf_errno()));
        return false;
    }

    if (hdr->e_machine != EM_ARM)
    {
        fprintf (stderr, "Sorry, this is not an ARM-binary\n");
        return false;
    }
    return true;
}


bool patch_elf (const char * filename, int CheckVector)
////////////////////////////////////////////////////////
{
    bool patch_success = false;

    if (elf_version(EV_CURRENT) == EV_NONE)
    {
       fprintf (stderr, "ELF library too old\n");
       return false;
    }

    int fd = open (filename, O_RDWR);
    if (fd <0)
    {
        fprintf (stderr, "unable to open file %s\n", filename);
        return false;
    }

    Elf *e = elf_begin(fd, ELF_C_RDWR, (Elf *) 0);
    if (!e)
    {
        fprintf (stderr, "elf_begin failed %s\n", elf_errmsg(elf_errno()));
        return false;
    }

    // We don't want libelf to mess with our layout in any way!
    // libelf may break arm binaries otherwise.
    if (!elf_flagelf (e, ELF_C_SET, ELF_F_LAYOUT))
    {
        fprintf (stderr, "elf_flagelf failed %s\n", elf_errmsg(elf_errno()));
        return false;
    }

    if (!check_arm(e))
    {
        return false;
    }

    // find the code-section:
    Elf_Scn * section = 0;
    while ((section = elf_nextscn(e, section)) != 0)
    {
        Elf32_Shdr * hdr = elf32_getshdr (section);
        if (!hdr)
        {
            fprintf (stderr, "elf32_getshdr failed %s\n", elf_errmsg(elf_errno()));
            return false;
        }

        if ( (hdr->sh_flags & SHF_ALLOC)     &&
             (hdr->sh_flags & SHF_EXECINSTR) &&
             (hdr->sh_addr == 0)             &&
             (hdr->sh_size >= NUMVECTORS*sizeof(uint32_t)))
        {
            Elf_Data *data = elf_getdata (section, 0);

            if (!data->d_buf)
            {
                fprintf (stderr, "executable section seems to be empty\n");
                return false;
            }

            uint32_t signature[NUMVECTORS];

            memcpy (signature, data->d_buf, NUMVECTORS * sizeof (uint32_t));
            uint32_t cksum = get_lpc_signature(signature, NUMVECTORS, CheckVector);

            printf ("old checksum: %08x\n", signature[CheckVector]);
            printf ("new checksum: %08x\n", cksum);
            signature[CheckVector] = cksum;

            // tag data-chunk as dirty. This forces recalculation of the elf checksum.
            if (!elf_flagdata (data, ELF_C_SET, ELF_F_DIRTY))
            {
                fprintf (stderr, "elf_flagdata failed %s\n", elf_errmsg(elf_errno()));
                return false;
            }

            // update ELF-file:
            memcpy (data->d_buf, signature, NUMVECTORS * sizeof (uint32_t));

            if (elf_update(e, ELF_C_WRITE) == -1)
            {
                fprintf (stderr, "elf_update failed %s\n", elf_errmsg(elf_errno()));
                return false;
            }

            // we're done:
            patch_success = true;
            break;
        }
    }

    elf_end (e);
    close (fd);
    return patch_success;
}


void help ()
////////////
{
    printf ("Usage: lpcpatchelf -f file.elf [-c PositionOfChecksum]\n");
    printf ("\n");
    printf ("   PositionOfChecksum:\n");
    printf ("\n");
    printf ("     The -c option tells where to place the checksum into the interrupt\n");
    printf ("     table. Default is 7 for LPC17xx and most other LPC microcontrollers\n");
    printf ("     for the LPC2000 family use uses position 5\n");
    printf ("\n");
    printf ("\n");
    printf ("lpcpatchelf - Updates LPC ARM processor specific checksum in elf-binaries.\n");
    printf ("\n");
    printf ("Copyright (C) 2016 Nils Pipenbrinck Email: n.pipenbrinck@hilbert-space.de\n");
    printf ("Version %d.%d, compiled %s\n", VERSION_MAJ, VERSION_MIN, __DATE__);
    printf ("\n");
    printf ("This program is free software; you can redistribute it and/or\n");
    printf ("modify it under the terms of the GNU General Public License\n");
    printf ("as published by the Free Software Foundation; either version 2\n");
    printf ("of the License, or (at your option) any later version.\n");
}


int main (int argc, char **argv)
/////////////////////////////////
{
    const char * elf_file = 0;
    int c;

    // Defaults for LPC17xx and LPC43xx familiy
    int CheckVector = 7;

    while ((c = getopt (argc, argv, "f:n:c:")) != -1)
    {
        switch (c)
        {
          case 'f':
            elf_file = optarg;
            break;

          case 'c':
            CheckVector = atol(optarg);
            break;

          case '?':
            if ((optopt == 'f') || (optopt == 'c'))
            {
                fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            }
            else if (isprint (optopt))
            {
                fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            }
            else
            {
                fprintf (stderr,"Unknown option character `\\x%x'.\n", optopt);
            }
            help();
            return EXIT_FAILURE;

          default:
            help();
            abort ();
        }
    }

    if ((CheckVector <0) || (CheckVector >= NUMVECTORS))
    {
        fprintf (stderr, "illegal PositionOfChecksum value\n");
        return EXIT_FAILURE;
    }

    if (elf_file)
    {
        if (patch_elf(elf_file, CheckVector))
        {
            return EXIT_SUCCESS;
        }
        else
        {
            fprintf (stderr, "something failed, probably you've passed an .elf file that this program doesn't understand\n");
            return EXIT_FAILURE;
        }
    }
    else
    {
        help();
        return EXIT_FAILURE;
    }
}

/*EOF */
