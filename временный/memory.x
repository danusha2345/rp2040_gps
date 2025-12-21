MEMORY
{
    FLASH (rx) : ORIGIN = 0x10000000, LENGTH = 2M    /* основная флеш XIP */
    SRAM (rwx) : ORIGIN = 0x20000000, LENGTH = 264K  /* RAM RP2040 */
}
SECTIONS
{
    .my_section 0x10040000 (NOLOAD):   /* пользовательская секция во флеше для хранения состояния (режим/цвет) */
    {
        *(.my_section)
    } > FLASH
}
