/* Room for the SCE module data at the end of the code segment.
 *
 * vita-elf-create writes the module info, imports and exports immediately
 * after the last byte of the segment holding the entry point, and refuses
 * to run when the next segment starts sooner:
 *
 *   Cannot allocate 2824 bytes for SCE data at end of segment 0;
 *   segment 1 overlaps
 *
 * ld aligns the data segment to 64K, so the space available is whatever
 * is left of the last page -- a number nobody chose, uniform over
 * 0..65535, redrawn by any change to the code size.  The launcher drew
 * 2388 bytes and the build stopped; the engine drew 9020 and passed.  A
 * project that links or not depending on that is a project that breaks at
 * random.
 *
 * Advancing the location counter before the first read-write section
 * pushes the data segment out without belonging to either segment, so the
 * code segment's size is unchanged and the gap is at least 16K whatever
 * the code size happens to be.  The cost is address space, not file size:
 * nothing is written for the hole.
 */
SECTIONS
{
  . = . + 0x4000;
} INSERT BEFORE .init_array;
