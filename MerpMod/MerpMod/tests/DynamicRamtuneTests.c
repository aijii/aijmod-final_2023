/*
    Copyright (C) 2020 Shamit Som shamitsom@gmail.com

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/

//Tests for Dynamic RAM Tuning
#include "EcuHacks.h"

#if DYN_RAMTUNING

extern void ClearRam();

//the test below assumes:
// - the `MerpMod.x` patch file is downloaded into HEW first,
// - the patched ROM is downloaded immediately after
// - the PC is manually set to the first line of the function manually

void Pull2DFloatTests() __attribute__ ((section ("Misc")));
void Pull3DFloatTests() __attribute__ ((section ("Misc")));

//-----------------------------------------------------------------------------
//  Check Pull2DFloat
//-----------------------------------------------------------------------------
void Pull2DFloatTests()
{
    ClearRam();
    PopulateRamVariables();

    int i;
    float returned = 0.0f;
    float expected = 0.0f;

    //initialize RAM tuning by setting some max number of tables
    pRamVariables.MaxDynRAMTables = 4;
    pRamVariables.ROMtoRAMArrayOffset = (pRamVariables.MaxDynRAMTables - 1)<<2;
    unsigned long *ROMHeaderArray = &(pRamVariables.MaxDynRAMTables) + 1;
    unsigned long *RAMHeaderArray = &(ROMHeaderArray[pRamVariables.MaxDynRAMTables]);

    TwoDTable *table;
    int tableType;
    short numCols;
    float *TableCols;
    float mult;
    float offs;

    //Verify using values directly from ROM
    table = (TwoDTable*) tFrontO2Scaling;
    numCols = table->columnCount;
    tableType = table->tableType;
    TableCols = table->columnHeaderArray;
    mult = table->multiplier;
    offs = table->offset;

    //uncomment the type of table you're checking
    //e.g. Base Timing is a 1-byte table, so uncomment the `char` table

    // char *Table_Y_ROM = (char*) table->tableCells;
    // short *Table_Y_ROM = (short*) table->tableCells;
    float *Table_Y_ROM = (float*) table->tableCells;

    // unsigned char *testTable = (unsigned char*) (&(RAMHeaderArray[pRamVariables.MaxDynRAMTables]));
    // unsigned short *testTable = (unsigned short*) (&(RAMHeaderArray[pRamVariables.MaxDynRAMTables]));
    float *testTable = (float*) (&(RAMHeaderArray[pRamVariables.MaxDynRAMTables]));

    // unsigned char Table_Y_RAM_1[numCols];
    // unsigned char Table_Y_RAM_2[numCols];
    // unsigned short Table_Y_RAM_1[numCols];
    // unsigned short Table_Y_RAM_2[numCols];
    float Table_Y_RAM_1[numCols];
    float Table_Y_RAM_2[numCols];

    //arbitrary tables to populate in RAM, generated with different values
    //calculated from the original ROM tables
    for(i = 0; i < numCols; i++){
        Table_Y_RAM_1[i] = Table_Y_ROM[i]*2.0;
        Table_Y_RAM_2[i] = Table_Y_ROM[i]*0.5;
    }

    //Check that Pull2D pulls from regular ROM table when RAM tuning
    //hasn't been initialized yet
    pRamVariables.MaxDynRAMTables = 0;
    pRamVariables.ROMtoRAMArrayOffset = (pRamVariables.MaxDynRAMTables - 1)<<2;
    for(i = 0; i < numCols; i++){
        returned = Pull2DHooked(table, TableCols[i]);
        expected = Table_Y_ROM[i];
        if(tableType){
            expected *= mult;
            expected += offs;
        }
        Assert(
            AreCloseEnough(returned, expected),
            "Pull2DHooked: Incorrect value, ROM Table "
            "(RAM tune not initialized)"
        );
    }

    //ensure values still pulled from ROM with RAM tune initialized, but
    //no tables allocated
    pRamVariables.MaxDynRAMTables = 4;
    pRamVariables.ROMtoRAMArrayOffset = (pRamVariables.MaxDynRAMTables - 1)<<2;
    for(i = 0; i < numCols; i++){
        returned = Pull2DHooked(table, TableCols[i]);
        expected = Table_Y_ROM[i];
        if(tableType){
            expected *= mult;
            expected += offs;
        }
        Assert(
            AreCloseEnough(returned, expected),
            "Pull2DHooked: Incorrect value, 4-byte table, ROM table "
            "(no RAM tables allocated)"
        );
    }

    //create an INVALID header for a allocated RAM table to be allocated
    //
    //ROM table is indexed increasing from base (0th element is _first_ in array)
    //RAM table is addressed decreasing from end (0th element is _last_ in array)
    //
    //RAM table entry looks like 0xLLLLOOOO where "OOOO" is the lower 2 bytes
    //of the table's base address, "LLLL" are flags. The RAM table is considered
    //valid when the most-significant byte is set equal to 0xFF. Both upper
    //bytes can be used by a connected PC for any desired flags when table is
    //invalid, however since the ECU only checks the MSB for validity, it must
    //be ensured that the lower byte is set to 0xFF ONLY if ALL of the upper flag
    //bytes are also set to 0xFF, otherwise an undefined address will be used to
    //pull data from, which is obviously not good.
    //
    //When valid, the full dword is just the RAM address to be used. This makes
    //for a very efficient way for the ECU to check table validity without wasting
    //precious ECU cycles, and it makes the organization of the dynamic RAM tables
    //nearly trivial.
    ROMHeaderArray[0] = (long) Table_Y_ROM;
    RAMHeaderArray[0] = (
        (long) testTable & 0xAAAAFFFF //arbitrary, non-valid flag bytes
    );

    //allocate a corresponding table in RAM with arbitrary values
    for(i = 0; i < numCols; i++){
        testTable[i] = Table_Y_RAM_1[i];
    }

    //ensure values still pulled from ROM with the invalid table header
    for(i = 0; i < numCols; i++){
        returned = Pull2DHooked(table, TableCols[i]);
        expected = Table_Y_ROM[i];
        if(tableType){
            expected *= mult;
            expected += offs;
        }
        Assert(
            AreCloseEnough(returned, expected),
            "Pull2DHooked: Incorrect value, ROM Table (invalid RAM table header)"
        );
    }

    //mark table as valid, ensure values now pulled from RAM
    RAMHeaderArray[0] |= 0xFFFF0000;
    for(i = 0; i < numCols; i++){
        returned = Pull2DHooked(table, TableCols[i]);
        expected = Table_Y_RAM_1[i];
        if(tableType){
            expected *= mult;
            expected += offs;
        }
        Assert(
            AreCloseEnough(returned, expected),
            "Pull2DHooked: Incorrect value, RAM Table (first header)"
        );
    }

    //check final header case
    testTable += numCols; //move to next free spot in RAM
    for(i = 0; i < numCols; i++){
        testTable[i] = Table_Y_RAM_2[i];
    }
    //clear original headers
    ROMHeaderArray[0] = 0x00000000;
    RAMHeaderArray[0] = 0xFFFFFFFF;

    //create new header
    ROMHeaderArray[pRamVariables.MaxDynRAMTables - 1] = (long) Table_Y_ROM;
    RAMHeaderArray[pRamVariables.MaxDynRAMTables - 1] = (long) testTable;

    for(i = 0; i < numCols; i++){
        returned = Pull2DHooked(table, TableCols[i]);
        expected = Table_Y_RAM_2[i];
        if(tableType){
            expected *= mult;
            expected += offs;
        }
        Assert(
            AreCloseEnough(returned, expected),
            "Pull2DHooked: Incorrect value, 4-byte table, RAM Table (last header)"
        );
    }

    asm("nop"); //breakpoint handle to check Pull2DFloat Patch
}

//-----------------------------------------------------------------------------
//  Check Pull3DFloat
//-----------------------------------------------------------------------------
void Pull3DFloatTests(){

    ClearRam();
    PopulateRamVariables();

    int i, j;
    float returned = 0.0f;
    float expected = 0.0f;

    //initialize RAM tuning by setting some max number of tables
    pRamVariables.MaxDynRAMTables = 4;
    pRamVariables.ROMtoRAMArrayOffset = (pRamVariables.MaxDynRAMTables - 1)<<2;
    unsigned long *ROMHeaderArray = &(pRamVariables.MaxDynRAMTables) + 1;
    unsigned long *RAMHeaderArray = &(ROMHeaderArray[pRamVariables.MaxDynRAMTables]);

    ThreeDTable *table;
    int tableType;
    short numCols;
    short numRows;
    float *TableCols;
    float *TableRows;
    float mult;
    float offs;

    table = (ThreeDTable*) tBaseTimingPNonCruise;
    tableType = table->tableType;
    numCols = table->columnCount;
    numRows = table->rowCount;
    TableCols = table->columnHeaderArray;
    TableRows = table->rowHeaderArray;
    mult = table->multiplier;
    offs = table->offset;

    //uncomment the type of table you're checking
    //e.g. Base Timing is a 1-byte table, so uncomment the `char` table

    unsigned char *Table_Z_ROM = (unsigned char*) (table->tableCells);
    // unsigned short *Table_Z_ROM = (unsigned short*) (table->tableCells);
    // float *Table_Z_ROM = (float*) (table->tableCells);

    unsigned char *testTable = (unsigned char*) (&(RAMHeaderArray[pRamVariables.MaxDynRAMTables]));
    // unsigned short *testTable = (unsigned short*) (&(RAMHeaderArray[pRamVariables.MaxDynRAMTables]));
    // float *testTable = (float*) (&(RAMHeaderArray[pRamVariables.MaxDynRAMTables]));

    unsigned char Table_Z_RAM_1[numCols*numRows];
    unsigned char Table_Z_RAM_2[numCols*numRows];
    // unsigned short Table_Z_RAM_1[numCols*numRows];
    // unsigned short Table_Z_RAM_2[numCols*numRows];
    // float Table_Z_RAM_1[numCols*numRows];
    // float Table_Z_RAM_2[numCols*numRows];

    //arbitrary tables to populate in RAM, generated with different values
    //calculated from the original ROM tables
    for(i = 0; i < numRows; i++){
        for(j = 0; j < numCols; j++){
            if(tableType){
                Table_Z_RAM_1[i*numCols + j] = Table_Z_ROM[i*numCols + j] << 1;
                Table_Z_RAM_2[i*numCols + j] = Table_Z_ROM[i*numCols + j] << 2;
            }
            else{
                Table_Z_RAM_1[i*numCols + j] = Table_Z_ROM[i*numCols + j]*2.0;
                Table_Z_RAM_2[i*numCols + j] = Table_Z_ROM[i*numCols + j]*4.0;
            }
        }
    }

    //Check that Pull3D pulls from regular ROM table when no RAM tuning
    //hasn't been initialized yet
    pRamVariables.MaxDynRAMTables = 0;
    pRamVariables.ROMtoRAMArrayOffset = (pRamVariables.MaxDynRAMTables - 1)<<2;
    for(i = 0; i < numRows; i++){
        for(j = 0; j < numCols; j++){
            returned = Pull3DHooked(table, TableCols[j], TableRows[i]);
            expected = Table_Z_ROM[i*numCols + j];
            if(tableType){
                expected *= mult;
                expected += offs;
            }
            Assert(
                AreCloseEnough(returned, expected),
                "Pull3DHooked: Incorrect value, ROM Table "
                "(RAM tune not initialized)"
            );
        }
    }

    //ensure values still pulled from ROM with RAM tune initialized, but
    //no tables allocated
    pRamVariables.MaxDynRAMTables = 4;
    pRamVariables.ROMtoRAMArrayOffset = (pRamVariables.MaxDynRAMTables - 1)<<2;
    for(i = 0; i < numRows; i++){
        for(j = 0; j < numCols; j++){
            returned = Pull3DHooked(table, TableCols[j], TableRows[i]);
            expected = Table_Z_ROM[i*numCols + j];
            if(tableType){
                expected *= mult;
                expected += offs;
            }
            Assert(
                AreCloseEnough(returned, expected),
                "Pull3DHooked: Incorrect value, ROM table "
                "(no RAM tables allocated)"
            );
        }
    }

    //create an INVALID header for a allocated RAM table to be allocated
    ROMHeaderArray[0] = (long) Table_Z_ROM;
    RAMHeaderArray[0] = (
        (long) testTable & 0xAAAAFFFF //arbitrary, non-valid flag bytes
    );

    //allocate a corresponding table in RAM with arbitrary values
    for(i = 0; i < numRows; i++){
        for(j = 0; j < numCols; j++){;
            testTable[i*numCols + j] = Table_Z_RAM_1[i*numCols + j];
        }
    }

    //ensure values still pulled from ROM with the invalid table header
    for(i = 0; i < numRows; i++){
        for(j = 0; j < numCols; j++){
            returned = Pull3DHooked(table, TableCols[j], TableRows[i]);
            expected = Table_Z_ROM[i*numCols + j];
            if(tableType){
                expected *= mult;
                expected += offs;
            }
            Assert(
                AreCloseEnough(returned, expected),
                "Pull3DHooked: Incorrect value, ROM Table (Invalid RAM Header)"
            );
        }
    }

    //mark table as valid, ensure values now pulled from RAM
    RAMHeaderArray[0] |= 0xFFFF0000;
    for(i = 0; i < numRows; i++){
        for(j = 0; j < numCols; j++){
            returned = Pull3DHooked(table, TableCols[j], TableRows[i]);
            expected = Table_Z_RAM_1[i*numCols + j];
            if(tableType){
                expected *= mult;
                expected += offs;
            }
            Assert(
                AreCloseEnough(returned, expected),
                "Pull3DHooked: Incorrect value, RAM Table (first header)"
            );
        }
    }

    //check final header case
    testTable += numCols*numRows; //move to next free spot in RAM
    for(i = 0; i < numRows; i++){
        for(j = 0; j < numCols; j++){
            testTable[i*numCols + j] = Table_Z_RAM_2[i*numCols + j];
        }
    }

    //clear original headers
    ROMHeaderArray[0] = 0x00000000;
    RAMHeaderArray[0] = 0x00000000;

    //create new header
    ROMHeaderArray[pRamVariables.MaxDynRAMTables - 1] = (long) Table_Z_ROM;
    RAMHeaderArray[pRamVariables.MaxDynRAMTables - 1] = (long) testTable;

    for(i = 0; i < numRows; i++){
        for(j = 0; j < numCols; j++){
            returned = Pull3DHooked(table, TableCols[j], TableRows[i]);
            expected = Table_Z_RAM_2[i*numCols + j];
            if(tableType){
                expected *= mult;
                expected += offs;
            }
            Assert(
                AreCloseEnough(returned, expected),
                "Pull3DHooked: Incorrect value, RAM Table (last header)"
            );
        }
    }

    asm("nop"); //breakpoint handle to check Pull3DFloat patch
}

#endif
