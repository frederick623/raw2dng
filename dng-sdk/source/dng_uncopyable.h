/*****************************************************************************/
// Copyright 2012-2019 Adobe Systems Incorporated
// All Rights Reserved.
//
// NOTICE:	Adobe permits you to use, modify, and distribute this file in
// accordance with the terms of the Adobe license agreement accompanying it.
/*****************************************************************************/

#ifndef __dng_uncopyable__
#define __dng_uncopyable__

/*****************************************************************************/

// Virtual base class to prevent object copies.

class dng_uncopyable
	{

	protected:

		dng_uncopyable ()
			{
			}

		~dng_uncopyable ()
			{
			}

		dng_uncopyable (const dng_uncopyable &)=delete;
		
		dng_uncopyable & operator= (const dng_uncopyable &)=delete;

        // Enable moving
        dng_uncopyable(dng_uncopyable&&) = default;

        dng_uncopyable& operator=(dng_uncopyable&&) = default;
		
	};

/*****************************************************************************/

#endif	// __dng_uncopyable__
	
/*****************************************************************************/
