# SPDX-License-Identifier: GPL-2.0+
# Copyright 2022 Google LLC
# Written by Simon Glass <sjg@chromium.org>
#
# Entry-type module for Open Portable Trusted Execution Environment (OP-TEE)
#

from binman.etype.blob_named_by_arg import Entry_blob_named_by_arg

class Entry_op_tee(Entry_blob_named_by_arg):
    """Open Portable Trusted Execution Environment (OP-TEE) blob

    Properties / Entry arguments:
        - op-tee-path: Filename of file to read into entry. This is typically
            called tee.elf

    This entry holds the OP-TEE Elf file, typically started by U-Boot SPL.
    See the U-Boot README for your architecture or board for how to use it. See
    https://https://www.op-tee.org/ for more information about OP-TEE.
    """
    def __init__(self, section, etype, node):
        super().__init__(section, etype, node, 'op-tee')
        self.external = True
