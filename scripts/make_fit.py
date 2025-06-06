#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0+
#
# Copyright 2024 Google LLC
# Written by Simon Glass <sjg@chromium.org>
#

"""Build a FIT containing a lot of devicetree files

Usage:
    make_fit.py -A arm64 -n 'Linux-6.6' -O linux
        -o arch/arm64/boot/image.fit -k /tmp/kern/arch/arm64/boot/image.itk
        @arch/arm64/boot/dts/dtbs-list -E -c gzip

Creates a FIT containing the supplied kernel and a set of devicetree files,
either specified individually or listed in a file (with an '@' prefix).

Use -E to generate an external FIT (where the data is placed after the
FIT data structure). This allows parsing of the data without loading
the entire FIT.

Use -c to compress the data, using bzip2, gzip, lz4, lzma, lzo and
zstd algorithms.

Use -D to decompose "composite" DTBs into their base components and
deduplicate the resulting base DTBs and DTB overlays. This requires the
DTBs to be sourced from the kernel build directory, as the implementation
looks at the .cmd files produced by the kernel build.

The resulting FIT can be booted by bootloaders which support FIT, such
as U-Boot, Linuxboot, Tianocore, etc.

Note that this tool does not yet support adding a ramdisk / initrd.
"""

import argparse
import collections
import os
import subprocess
import sys
import tempfile
import time

import libfdt


# Tool extension and the name of the command-line tools
CompTool = collections.namedtuple('CompTool', 'ext,tools')

COMP_TOOLS = {
    'bzip2': CompTool('.bz2', 'bzip2'),
    'gzip': CompTool('.gz', 'pigz,gzip'),
    'lz4': CompTool('.lz4', 'lz4'),
    'lzma': CompTool('.lzma', 'lzma'),
    'lzo': CompTool('.lzo', 'lzop'),
    'zstd': CompTool('.zstd', 'zstd'),
}


def parse_args():
    """Parse the program ArgumentParser

    Returns:
        Namespace object containing the arguments
    """
    epilog = 'Build a FIT from a directory tree containing .dtb files'
    parser = argparse.ArgumentParser(epilog=epilog, fromfile_prefix_chars='@')
    parser.add_argument('-A', '--arch', type=str, required=True,
          help='Specifies the architecture')
    parser.add_argument('--dtb-compress', type=str, default='none',
          help='Specifies the compression of the DTBs')
    parser.add_argument('-D', '--decompose-dtbs', action='store_true',
          help='Decompose composite DTBs into base DTB and overlays')
    parser.add_argument('-E', '--external', action='store_true',
          help='Convert the FIT to use external data')
    parser.add_argument('-n', '--name', type=str, required=True,
          help='Specifies the name')
    parser.add_argument('-o', '--output', type=str, required=True,
          help='Specifies the output file (.fit)')
    parser.add_argument('-O', '--os', type=str, required=True,
          help='Specifies the operating system')
    parser.add_argument('-k', '--kernel', type=str, required=True,
          help='Specifies the (uncompressed) kernel input file (.itk)')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='Enable verbose output')
    parser.add_argument('dtbs', type=str, nargs='*',
          help='Specifies the devicetree files to process')

    return parser.parse_args()


def setup_fit(fsw, name):
    """Make a start on writing the FIT

    Outputs the root properties and the 'images' node

    Args:
        fsw (libfdt.FdtSw): Object to use for writing
        name (str): Name of kernel image
    """
    fsw.INC_SIZE = 65536
    fsw.finish_reservemap()
    fsw.begin_node('')
    fsw.property_string('description', f'{name} with devicetree set')
    fsw.property_u32('#address-cells', 1)

    fsw.property_u32('timestamp', int(time.time()))
    fsw.begin_node('images')


def write_kernel(fsw, data, args):
    """Write out the kernel image

    Writes a kernel node along with the required properties

    Args:
        fsw (libfdt.FdtSw): Object to use for writing
        data (bytes): Data to write (possibly compressed)
        args (Namespace): Contains necessary strings:
            arch: FIT architecture, e.g. 'arm64'
            fit_os: Operating Systems, e.g. 'linux'
            name: Name of OS, e.g. 'Linux-6.6.0-rc7'
            compress: Compression algorithm to use, e.g. 'gzip'
    """
    with fsw.add_node('kernel'):
        fsw.property_string('description', args.name)
        fsw.property_string('type', 'kernel_noload')
        fsw.property_string('arch', args.arch)
        fsw.property_string('os', args.os)
        fsw.property_string('compression', 'none')
        fsw.property('data', data)
        fsw.property_u32('load', 0)
        fsw.property_u32('entry', 0)


def finish_fit(fsw, entries):
    """Finish the FIT ready for use

    Writes the /configurations node and subnodes

    Args:
        fsw (libfdt.FdtSw): Object to use for writing
        entries (list of tuple): List of configurations:
            str: Description of model
            str: Compatible stringlist
    """
    fsw.end_node()
    with fsw.add_node('configurations'):
        for dtbname, model, compat, files in entries:
            with fsw.add_node(f'conf-{dtbname}'):
                fsw.property('compatible', bytes(compat))
                fsw.property_string('description', model)
                fsw.property('fdt', bytes(''.join(f'fdt-{x}\x00' for x in files), "ascii"))
                fsw.property_string('kernel', 'kernel')
    fsw.end_node()


def compress_data(inf, compress):
    """Compress data using a selected algorithm

    Args:
        inf (IOBase): Filename containing the data to compress
        compress (str): Compression algorithm, e.g. 'gzip'

    Return:
        bytes: Compressed data
    """
    if compress == 'none':
        return inf.read()

    comp = COMP_TOOLS.get(compress)
    if not comp:
        raise ValueError(f"Unknown compression algorithm '{compress}'")

    with tempfile.NamedTemporaryFile() as comp_fname:
        with open(comp_fname.name, 'wb') as outf:
            done = False
            for tool in comp.tools.split(','):
                try:
                    subprocess.call([tool, '-c'], stdin=inf, stdout=outf)
                    done = True
                    break
                except FileNotFoundError:
                    pass
            if not done:
                raise ValueError(f'Missing tool(s): {comp.tools}\n')
            with open(comp_fname.name, 'rb') as compf:
                comp_data = compf.read()
    return comp_data


def output_dtb(fsw, seq, fname, arch, compress):
    """Write out a single devicetree to the FIT

    Args:
        fsw (libfdt.FdtSw): Object to use for writing
        seq (int): Sequence number (1 for first)
        fname (str): Filename containing the DTB
        arch: FIT architecture, e.g. 'arm64'
        compress (str): Compressed algorithm, e.g. 'gzip'
    """
    with fsw.add_node(f'fdt-{seq}'):
        fsw.property_string('description', os.path.basename(fname))
        fsw.property_string('type', 'flat_dt')
        fsw.property_string('arch', arch)
        fsw.property_string('compression', compress)

        with open(fname, 'rb') as inf:
            compressed = compress_data(inf, compress)
        fsw.property('data', compressed)


def process_dtb(fname, args):
    """Process an input DTB, decomposing it if requested and is possible

    Args:
        fname (str): Filename containing the DTB
        args (Namespace): Program arguments
    Returns:
        tuple:
            str: Model name string
            str: Root compatible string
            files: list of filenames corresponding to the DTB
    """
    # Get the compatible / model information
    with open(fname, 'rb') as inf:
        data = inf.read()
    fdt = libfdt.FdtRo(data)
    model = fdt.getprop(0, 'model').as_str()
    compat = fdt.getprop(0, 'compatible')

    if args.decompose_dtbs:
        # Check if the DTB needs to be decomposed
        path, basename = os.path.split(fname)
        cmd_fname = os.path.join(path, f'.{basename}.cmd')
        with open(cmd_fname, 'r', encoding='ascii') as inf:
            cmd = inf.read()

        if 'scripts/dtc/fdtoverlay' in cmd:
            # This depends on the structure of the composite DTB command
            files = cmd.split()
            files = files[files.index('-i') + 1:]
        else:
            files = [fname]
    else:
        files = [fname]

    return (model, compat, files)

def build_fit(args):
    """Build the FIT from the provided files and arguments

    Args:
        args (Namespace): Program arguments

    Returns:
        tuple:
            bytes: FIT data
            int: Number of configurations generated
            size: Total uncompressed size of data
    """
    seq = 0
    size = 0
    fsw = libfdt.FdtSw()
    setup_fit(fsw, args.name)
    entries = []
    dtbs_seen = set()
    fdts = {}

    for fname in args.dtbs:
        # Ignore non-DTB (*.dtb) files
        if os.path.splitext(fname)[1] != '.dtb':
            continue

        (model, compat, files) = process_dtb(fname, args)

        for fn in files:
            if fn not in fdts:
                seq += 1
                size += os.path.getsize(fn)
                output_dtb(fsw, seq, fn, args.arch, args.dtb_compress)
                fdts[fn] = seq

        files_seq = [fdts[fn] for fn in files]

        dtbname = os.path.basename(fname)
        ndtbs_seen = len(dtbs_seen)
        dtbs_seen.add(dtbname)
        if len(dtbs_seen) == ndtbs_seen:
            raise RuntimeError(f"Duplicate file name '{dtbname}' during FIT creation")

        entries.append([dtbname, model, compat, files_seq])

    # Handle the kernel
    with open(args.kernel, 'rb') as inf:
        comp_data = compress_data(inf, 'none')
    size += os.path.getsize(args.kernel)
    write_kernel(fsw, comp_data, args)

    finish_fit(fsw, entries)

    # Include the kernel itself in the returned file count
    return fsw.as_fdt().as_bytearray(), seq + 1, size


def run_make_fit():
    """Run the tool's main logic"""
    args = parse_args()

    out_data, count, size = build_fit(args)
    with open(args.output, 'wb') as outf:
        outf.write(out_data)

    ext_fit_size = None
    if args.external:
        mkimage = os.environ.get('MKIMAGE', 'mkimage')
        subprocess.check_call([mkimage, '-E', '-F', args.output],
                              stdout=subprocess.DEVNULL)

        with open(args.output, 'rb') as inf:
            data = inf.read()
        ext_fit = libfdt.FdtRo(data)
        ext_fit_size = ext_fit.totalsize()

    if args.verbose:
        comp_size = len(out_data)
        print(f'FIT size {comp_size:#x}/{comp_size / 1024 / 1024:.1f} MB',
              end='')
        if ext_fit_size:
            print(f', header {ext_fit_size:#x}/{ext_fit_size / 1024:.1f} KB',
                  end='')
        print(f', {count} files, uncompressed {size / 1024 / 1024:.1f} MB')


if __name__ == "__main__":
    sys.exit(run_make_fit())
