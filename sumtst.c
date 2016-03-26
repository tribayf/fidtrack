/*
 *  sumtst.c
 *  sumtst (a.k.a. fidtrack)
 *
 *  A Max/MSP external that receives tracking information, containing 
 *  the fiducial marker id, the ordering information, x and y coordinates
 *  and the rotation angle of the fiducials from the TUIO client object
 *  and computes the audio processing parameters from these control data.
 *
 *  Copyright 2010 Tristan Bayfield, Atakan Gunal
 *
 *	This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#define MAXDIST 0.34


#include "ext.h"		// Standard Max include, always required
#include "ext_obex.h"	// required for new style Max object
#include <math.h>
#include <stdlib.h>

// Object Struct
typedef struct _sumtst 
{
	t_object ob;
	void	 *outlet;
} t_sumtst;

typedef struct _fiducial
{
	int fid_id;
	double session_id;
	bool  active;
	double angle;
	double x;
	double y;
	double freq;
	double amp;
	int target;
	
} t_fiducial;

t_fiducial fid_arr[6];
double dist_matrix[6][6];
t_atom list[30];

// Function Prototypes
void *sumtst_new(t_symbol *s, long argc, t_atom *argv);
void sumtst_free(t_sumtst *x);

void sumtst_addObject(t_sumtst *x, t_symbol *s, long argc, t_atom *argv);
void sumtst_updateObject(t_sumtst *x, t_symbol *s, long argc, t_atom *argv);
void sumtst_removeObject(t_sumtst *x, t_symbol *s, long argc, t_atom *argv);

void matrix_update();
void update_targets();
void update_amplitudes();
void make_list();


// Global Class Pointer Variable
void *sumtst_class;


int main(void)
{	
	t_class *c = NULL;
	
	c = class_new( "sumtst", (method) sumtst_new, (method) sumtst_free, (long) sizeof(t_sumtst), 0L, A_GIMME, 0);
	
	
	/* you CAN'T call this from the patcher */
	class_addmethod(c, (method) sumtst_addObject, "addObject", A_GIMME, 0);
	class_addmethod(c, (method) sumtst_updateObject, "updateObject", A_GIMME, 0);
	class_addmethod(c, (method) sumtst_removeObject, "removeObject", A_GIMME, 0);
	
	class_register(CLASS_BOX, c);
	sumtst_class = c;
	
	return 0;
}


void *sumtst_new(t_symbol *s, long argc, t_atom *argv)
{
	int i;
	t_sumtst *x = NULL;
	x = (t_sumtst *)object_alloc(sumtst_class);
	object_post((t_object *)x, "a new %s object was instantiated: 0x%X", s->s_name, x);
	object_post((t_object *)x, "it has %ld arguments", argc);
	x->outlet = outlet_new((t_object*) x, NULL); ///< Create an outlet, NULL means this is a generic outlet
	
	for (i = 0; i<6; i++)
	{
		fid_arr[i].fid_id = i;
		fid_arr[i].session_id = 10000.0;
		fid_arr[i].active = FALSE;
		fid_arr[i].angle = 0.0;
		fid_arr[i].x = 0.0;
		fid_arr[i].y = 0.0;
		fid_arr[i].freq = 0.0;
		fid_arr[i].amp = 0.0;
		fid_arr[i].target = -1;
	}
	

	return (x);
}


void sumtst_free(t_sumtst *x)
{
	// Nothing to do.
}



void sumtst_addObject(t_sumtst *x, t_symbol *s, long argc, t_atom *argv)
{
	t_atom	 *ap = argv;
	t_symbol *sym;
	int fid = atom_getlong(ap + 1);
	
	post("addObject received: %ld %ld %f %f", atom_getlong(ap), atom_getlong(ap +1), atom_getfloat(ap + 2), atom_getfloat(ap + 3));
	
	//set active
	fid_arr[fid].active = TRUE; 
	post("fid_arr[%ld].active set to %s", fid, fid_arr[fid].active? "ttrrue" : "ffaalse");
	
	//set session_id
	fid_arr[fid].session_id = atom_getlong(ap);
	
	//set x
	fid_arr[fid].x = atom_getfloat(ap + 2);
	post("fid_arr[%ld].x set to %f", fid, fid_arr[fid].x);
	
	//set y
	fid_arr[fid].y = atom_getfloat(ap + 3);
	post("fid_arr[%ld].y set to %f", fid, fid_arr[fid].y);
	
	//set angle
	fid_arr[fid].angle = atom_getfloat(ap + 4);
	post("fid_arr[%ld].y set to %f", fid, fid_arr[fid].angle);
	
	//matrix_update
	matrix_update();
	
	//update_targets
	update_targets();
	
	//set freq
	if (fid_arr[fid].target == 0)
	{
		fid_arr[fid].freq = 5900 / 6.2831 * fid_arr[fid].angle + 100;
	}
	else if (fid_arr[fid].target < 0)
	{
		fid_arr[fid].freq = 0;
	}
	else 
	{
		fid_arr[fid].freq = 396 / 6.2831 * fid_arr[fid].angle + 4;
	}

	//set amplitudes
	update_amplitudes();

	//makelist 
	make_list();
	
	sym = gensym("list");
	
	//output from outlet
	outlet_anything(x->outlet, sym, 30, list);
	
}

void sumtst_removeObject(t_sumtst *x, t_symbol *s, long argc, t_atom *argv)
{
	t_atom	 *ap = argv;
	t_symbol *sym;
	int fid = atom_getlong(ap + 1);
	
	post("addObject received: %ld %ld", atom_getlong(ap), atom_getlong(ap +1) );
	
	//set inactive
	fid_arr[fid].active = FALSE;
	post("fid_arr[%ld].active set to %s", fid, fid_arr[fid].active ? "ttrrue" : "ffaalse");
	
	//matrix_update
	matrix_update();
	
	//update_targets
	update_targets();
	
	//set amplitudes
	update_amplitudes();
	
	//makelist 
	make_list();
	
	sym = gensym("list");
	
	//output from outlet
	outlet_anything(x->outlet, sym, 30, list);
	
}

void sumtst_updateObject(t_sumtst *x, t_symbol *s, long argc, t_atom *argv)
{
	t_atom	 *ap = argv;
	t_symbol *sym;
	int fid = atom_getlong(ap + 1);
	
	post("updateObject received: %ld %ld %f %f", atom_getlong(ap), atom_getlong(ap +1), atom_getfloat(ap + 2), atom_getfloat(ap + 3));
	
	//set session_id
	fid_arr[fid].session_id = atom_getlong(ap);
	
	//set x
	fid_arr[fid].x = atom_getfloat(ap + 2);
	post("fid_arr[%ld].x set to %f", fid, fid_arr[fid].x);
	
	//set y
	fid_arr[fid].y = atom_getfloat(ap + 3);
	post("fid_arr[%ld].y set to %f", fid, fid_arr[fid].y);
	
	//set angle
	fid_arr[fid].angle = atom_getfloat(ap + 4);
	post("fid_arr[%ld].y set to %f", fid, fid_arr[fid].angle);
	
	//matrix_update
	matrix_update();
	
	//update_targets
	update_targets();
	
	//set freq
	if (fid_arr[fid].target == 0)
	{
		fid_arr[fid].freq = 5900 / 6.2831 * fid_arr[fid].angle + 100;
	}
	else if (fid_arr[fid].target < 0)
	{
		fid_arr[fid].freq = 0;
	}
	else 
	{
		fid_arr[fid].freq = 396 / 6.2831 * fid_arr[fid].angle + 4;
	}
	
	post("fid_arr[%ld].freq = %f", fid, fid_arr[fid].freq);
	
	//set amplitudes
	update_amplitudes();
	
	//makelist 
	make_list();
	
	sym = gensym("list");
	
	//output from outlet
	outlet_anything(x->outlet, sym, 30, list);
}

int sort(const void *x, const void *y) 
{
	return ((*(t_fiducial*)x).session_id - (*(t_fiducial*)y).session_id);
}

void matrix_update()
{
	int i, j;
	t_fiducial fid_sorted[6];
	//memmove(&fid_sorted, &fid_arr, sizeof(t_fiducial));
	for(i = 0; i < 6; i++)
	{
		fid_sorted[i] = fid_arr[i];
	}

	qsort(fid_sorted, 6, sizeof(t_fiducial), sort);
	
	post("fid_sorted: %d %d %d %d %d %d", fid_sorted[0].fid_id, fid_sorted[1].fid_id, fid_sorted[2].fid_id,
											fid_sorted[3].fid_id, fid_sorted[4].fid_id, fid_sorted[5].fid_id);
	
	//post("fid_sorted: %d %d %d %d %d %d", fid_arr[0].fid_id, fid_arr[1].fid_id, fid_arr[2].fid_id,
	//	 fid_arr[3].fid_id, fid_arr[4].fid_id, fid_arr[5].fid_id);
	
	for (i = 0; i<6; i++)
	{
		for (j = 0; j<6; j++)
		{
			dist_matrix[j][i] = -1;
		}
	}
	
//	for (i = 0; i<6; i++)
//	{
//		post("%f %f %f %f %f %f", dist_matrix[0][i], dist_matrix[1][i], dist_matrix[2][i],
//			 dist_matrix[3][i], dist_matrix[4][i], dist_matrix[5][i]);
//	}
			
  for (i = 0; i < 6; i++)
	{
		for (j = 0; j < i; j++)
		{
			if(fid_sorted[i-j-1].active && fid_sorted[i].active)
			{
				dist_matrix[fid_sorted[i].fid_id][fid_sorted[i-j-1].fid_id] = sqrt(
					pow(fid_sorted[i].x - fid_sorted[i-j-1].x, 2) +
					pow(fid_sorted[i].y - fid_sorted[i-j-1].y, 2) );											  															   
			}
		}
	}

	for (i = 0; i<6; i++)
	{
		post("%f %f %f %f %f %f", dist_matrix[0][i], dist_matrix[1][i], dist_matrix[2][i],
			 dist_matrix[3][i], dist_matrix[4][i], dist_matrix[5][i]);
	}
	
	
}

void update_targets()
{
	int i, j, last_session_id;
	
	for (i = 0; i < 6; i++)
	{
		fid_arr[i].target = 0;
		last_session_id = 0;
		
		for (j = 0; j < 6; j++)
		{
			if (dist_matrix[j][i] > -1 && dist_matrix[j][i] < MAXDIST)
			{
				if (fid_arr[i].session_id > last_session_id)
				{
					fid_arr[i].target = j + 1;
					last_session_id = fid_arr[j].session_id;
				}
			}
		}
	}	
}

void update_amplitudes()
{
	int i;
	
	for ( i = 0; i < 6; i++)
	{
		if (fid_arr[i].target == 0)
		{
			fid_arr[i].amp = 1;
		}
		else 
		{
			fid_arr[i].amp = -130.667 * (dist_matrix[i][fid_arr[i].target - 1] - 0.1) + 100;
		}

	}
}

void make_list()
{
	int i;
	int offset = 0;
	
	for (i = 0; i < 6; i++)
	{
		atom_setlong(list + (offset + 0), fid_arr[i].active ? 1 : 0);
		atom_setlong(list + (offset + 1), fid_arr[i].target);
		//atom_setlong(list + (offset + 2), pow(2, (ceil((i + 1)/2) - 1))); //wave type
		atom_setlong(list + (offset + 2), pow(2, floor(i/2))); //wave type
		atom_setfloat(list + (offset + 3), fid_arr[i].freq);
		atom_setfloat(list + (offset + 4), fid_arr[i].amp);
		
		offset += 5;
	}
	
}
