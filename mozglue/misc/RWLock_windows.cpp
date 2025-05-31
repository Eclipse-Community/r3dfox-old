/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XP_WIN
#  error This file should only be compiled on Windows.
#endif

#include "mozilla/PlatformRWLock.h"

#include <windows.h>

//  Reader / Writer Lock

typedef struct _SYNC_RW_LOCK {
    volatile DWORD  dwControlWord;
    volatile DWORD  cOwnerQuiesced;
    HANDLE          hsemWriter;
    HANDLE          hsemReader;
} SYNC_RW_LOCK, *PSYNC_RW_LOCK;

#define NativeHandle(m) (reinterpret_cast<SYNC_RW_LOCK*>(&m))

//  Utilities

__inline
VOID
SyncpPause(
    )
{
#if defined( _M_IX86 )

    __asm rep nop

#else  //  !_M_IX86
#endif  //  _M_IX86
}

#define SYNC_FOREVER for ( ; ; SyncpPause() )


//  Reader / Writer Lock

static VOID SyncpRWLUpdateQuiescedOwnerCountAsWriter(
    IN OUT  PSYNC_RW_LOCK       prwl,
    IN      const DWORD         cOwnerQuiescedDelta
    )
{
    DWORD cOwnerQuiescedBI;
    DWORD cOwnerQuiescedAI;
    DWORD dwControlWordBIExpected;
    DWORD dwControlWordAI;
    DWORD dwControlWordBI;
    
    //  update the quiesced owner count using the provided delta

    cOwnerQuiescedBI = InterlockedExchangeAdd( &prwl->cOwnerQuiesced, cOwnerQuiescedDelta );
    cOwnerQuiescedAI = cOwnerQuiescedBI + cOwnerQuiescedDelta;

    //  our update resulted in a zero quiesced owner count

    if ( !cOwnerQuiescedAI ) {
        
        //  we must release the waiting readers because we removed the last
        //  quiesced owner count

        //  try forever until we successfully change the lock state

        SYNC_FOREVER {
            
            //  read the current state of the control word as our expected before image

            dwControlWordBIExpected = prwl->dwControlWord;

            //  compute the after image of the control word such that we jump from state
            //  state 4 to state 1 or from state 5 to state 3, whichever is appropriate

            dwControlWordAI =   (DWORD)( dwControlWordBIExpected &
                                ( ( ( (LONG_PTR)( (long)( ( dwControlWordBIExpected + 0xFFFF7FFF ) << 16 ) ) >> 31 ) &
                                0xFFFF0000 ) ^ 0x8000FFFF ) );

            //  attempt to perform the transacted state transition on the control word

            dwControlWordBI = InterlockedCompareExchange( &prwl->dwControlWord, dwControlWordAI, dwControlWordBIExpected );

            //  the transaction failed because another context changed the control word

            if ( dwControlWordBI != dwControlWordBIExpected ) {
                
                //  try again

                continue;

            //  the transaction succeeded

            } else {
            
                //  we're done

                break;
            }
        }

        //  we just jumped from state 5 to state 3

        if ( dwControlWordBI & 0x00007FFF ) {
            
            //  update the quiesced owner count with the owner count that we displaced
            //  from the control word
            //
            //  NOTE:  we do not have to worry about releasing any more waiters because
            //  either this context owns one of the owner counts or at least one context
            //  that owns an owner count are currently blocked on the semaphore

            const DWORD cOwnerQuiescedDelta = ( dwControlWordBI & 0x7FFF0000 ) >> 16;
            InterlockedExchangeAdd( &prwl->cOwnerQuiesced, cOwnerQuiescedDelta );
        }

        //  release the waiting readers that we removed from the lock state

        ReleaseSemaphore( prwl->hsemReader, ( dwControlWordBI & 0x7FFF0000 ) >> 16, NULL );
    }
}

static VOID SyncpRWLUpdateQuiescedOwnerCountAsReader(
    IN OUT  PSYNC_RW_LOCK       prwl,
    IN      const DWORD         cOwnerQuiescedDelta
    )
{
    DWORD cOwnerQuiescedBI;
    DWORD cOwnerQuiescedAI;
    DWORD dwControlWordBIExpected;
    DWORD dwControlWordAI;
    DWORD dwControlWordBI;
    
    //  update the quiesced owner count using the provided delta

    cOwnerQuiescedBI = InterlockedExchangeAdd( &prwl->cOwnerQuiesced, cOwnerQuiescedDelta );
    cOwnerQuiescedAI = cOwnerQuiescedBI + cOwnerQuiescedDelta;

    //  our update resulted in a zero quiesced owner count

    if ( !cOwnerQuiescedAI ) {
        
        //  we must release a waiting writer because we removed the last
        //  quiesced owner count

        //  try forever until we successfully change the lock state

        SYNC_FOREVER {
            
            //  read the current state of the control word as our expected before image

            dwControlWordBIExpected = prwl->dwControlWord;

            //  compute the after image of the control word such that we jump from state
            //  state 3 to state 2, from state 5 to state 4, or from state 5 to state 5,
            //  whichever is appropriate

            dwControlWordAI =   dwControlWordBIExpected +
                                ( ( dwControlWordBIExpected & 0x7FFF0000 ) ? 0xFFFFFFFF : 0xFFFF8000 );

            //  attempt to perform the transacted state transition on the control word

            dwControlWordBI = InterlockedCompareExchange( &prwl->dwControlWord, dwControlWordAI, dwControlWordBIExpected );

            //  the transaction failed because another context changed the control word

            if ( dwControlWordBI != dwControlWordBIExpected ) {
                
                //  try again

                continue;

            //  the transaction succeeded

            } else {
            
                //  we're done

                break;
            }
        }

        //  we just jumped from state 5 to state 4 or from state 5 to state 5

        if ( dwControlWordBI & 0x7FFF0000 ) {
            
            //  update the quiesced owner count with the owner count that we displaced
            //  from the control word
            //
            //  NOTE:  we do not have to worry about releasing any more waiters because
            //  either this context owns one of the owner counts or at least one context
            //  that owns an owner count are currently blocked on the semaphore

            const DWORD cOwnerQuiescedDelta = 1;
            InterlockedExchangeAdd( &prwl->cOwnerQuiesced, cOwnerQuiescedDelta );
        }

        //  release the waiting writer that we removed from the lock state

        ReleaseSemaphore( prwl->hsemWriter, 1, NULL );
    }
}

static VOID SyncpRWLEnterAsWriter(
    IN OUT  PSYNC_RW_LOCK       prwl,
    IN      const DWORD         dwControlWordBI
    )
{
    //  we just jumped from state 1 to state 3

    if ( ( dwControlWordBI & 0x80008000 ) == 0x00008000 ) {
        
        //  update the quiesced owner count with the owner count that we displaced from
        //  the control word, possibly releasing a waiter.  we update the count as if we
        //  were a reader as a writer can be released

        SyncpRWLUpdateQuiescedOwnerCountAsReader( prwl, ( dwControlWordBI & 0x7FFF0000 ) >> 16 );
    }

    //  wait for ownership of the lock on our semaphore

    WaitForSingleObject( prwl->hsemWriter, INFINITE );
}

static VOID SyncpRWLEnterAsReader(
    IN OUT  PSYNC_RW_LOCK       prwl,
    IN      const DWORD         dwControlWordBI
    )
{
    //  we just jumped from state 2 to state 4 or from state 2 to state 5

    if ( ( dwControlWordBI & 0x80008000 ) == 0x80000000 ) {
        
        //  update the quiesced owner count with the owner count that we displaced from
        //  the control word, possibly releasing waiters.  we update the count as if we
        //  were a writer as readers can be released

        SyncpRWLUpdateQuiescedOwnerCountAsWriter( prwl, 0x00000001 );
    }

    //  wait for ownership of the lock on our semaphore

    WaitForSingleObject( prwl->hsemReader, INFINITE );
}

static VOID SyncCreateRWLock(
    OUT     PSYNC_RW_LOCK       prwl
    )
{
    memset( prwl, 0, sizeof( SYNC_RW_LOCK ) );

    prwl->hsemWriter    = CreateSemaphoreW( NULL, 0, 0x7FFFFFFF, NULL );
    prwl->hsemReader    = CreateSemaphoreW( NULL, 0, 0x7FFFFFFF, NULL );

    if ( !prwl->hsemWriter || !prwl->hsemReader ) {
        if ( prwl->hsemWriter ) {
            CloseHandle( prwl->hsemWriter );
            prwl->hsemWriter = NULL;
        }
        if ( prwl->hsemReader ) {
            CloseHandle( prwl->hsemReader );
            prwl->hsemReader = NULL;
        }
        RaiseException( STATUS_NO_MEMORY, EXCEPTION_NONCONTINUABLE, 0, NULL );
    }
}

static VOID SyncDestroyRWLock(
    IN      PSYNC_RW_LOCK       prwl
    )
{
    if ( prwl->hsemWriter ) {
        CloseHandle( prwl->hsemWriter );
        prwl->hsemWriter = NULL;
    }
    if ( prwl->hsemReader ) {
        CloseHandle( prwl->hsemReader );
        prwl->hsemReader = NULL;
    }
}

static VOID SyncEnterRWLockAsWriter(
    IN OUT  PSYNC_RW_LOCK       prwl
    )
{
    DWORD dwControlWordBIExpected;
    DWORD dwControlWordAI;
    DWORD dwControlWordBI;
    
    //  try forever until we successfully change the lock state

    SYNC_FOREVER {
        
        //  read the current state of the control word as our expected before image

        dwControlWordBIExpected = prwl->dwControlWord;

        //  compute the after image of the control word by performing the global
        //  transform for the EnterAsWriter state transition

        dwControlWordAI =   (DWORD)( ( ( dwControlWordBIExpected & ( ( (LONG_PTR)( (long)( dwControlWordBIExpected ) ) >> 31 ) |
                            0x0000FFFF ) ) | 0x80000000 ) + 0x00000001 );

        //  attempt to perform the transacted state transition on the control word

        dwControlWordBI = InterlockedCompareExchange( &prwl->dwControlWord, dwControlWordAI, dwControlWordBIExpected );

        //  the transaction failed or writers are quiesced from ownership or a
        //  writer already owns the lock

        if ( ( dwControlWordBI ^ dwControlWordBIExpected ) | ( dwControlWordBI & 0x0000FFFF ) ) {
            
            //  the transaction failed because another context changed the control word

            if ( dwControlWordBI != dwControlWordBIExpected ) {
                
                //  try again

                continue;

            //  the transaction succeeded but writers are quiesced from ownership
            //  or a writer already owns the lock

            } else {
            
                //  wait to own the lock as a writer

                SyncpRWLEnterAsWriter( prwl, dwControlWordBI );

                //  we now own the lock, so we're done

                break;
            }

        //  the transaction succeeded and writers were not quiesced from ownership
        //  and a writer did not already own the lock

        } else {
        
            //  we now own the lock, so we're done

            break;
        }
    }
}

static BOOL SyncTryEnterRWLockAsWriter(
    IN OUT  PSYNC_RW_LOCK       prwl
    )
{
    DWORD dwControlWordBIExpected;
    DWORD dwControlWordAI;
    DWORD dwControlWordBI;
    
    //  try forever until we successfully change the lock state

    SYNC_FOREVER {
        
        //  read the current state of the control word as our expected before image

        dwControlWordBIExpected = prwl->dwControlWord;

        //  change the expected before image so that the transaction will only work if
        //  writers were not quiesced from ownership and another writer doesn't already
        //  own the lock

        dwControlWordBIExpected = dwControlWordBIExpected & 0xFFFF0000;

        //  compute the after image of the control word by performing the global
        //  transform for the EnterAsWriter state transition

        dwControlWordAI =   (DWORD)( ( ( dwControlWordBIExpected & ( ( (LONG_PTR)( (long)( dwControlWordBIExpected ) ) >> 31 ) |
                            0x0000FFFF ) ) | 0x80000000 ) + 0x00000001 );

        //  attempt to perform the transacted state transition on the control word

        dwControlWordBI = InterlockedCompareExchange( &prwl->dwControlWord, dwControlWordAI, dwControlWordBIExpected );

        //  the transaction failed

        if ( dwControlWordBI != dwControlWordBIExpected ) {
            
            //  the transaction failed because writers were quiesced from ownership
            //  or another writer already owns the lock

            if ( dwControlWordBI & 0x0000FFFF ) {
                
                //  return failure

                return FALSE;

            //  the transaction failed because another context changed the control word

            } else {
            
                //  try again

                continue;
            }

        //  the transaction succeeded

        } else {
        
            // return success

            return TRUE;
        }
    }
}

static VOID SyncLeaveRWLockAsWriter(
    IN OUT  PSYNC_RW_LOCK       prwl
    )
{
    DWORD dwControlWordBIExpected;
    DWORD dwControlWordAI;
    DWORD dwControlWordBI;
    
    //  try forever until we successfully change the lock state

    SYNC_FOREVER {
        
        //  read the current state of the control word as our expected before image

        dwControlWordBIExpected = prwl->dwControlWord;

        //  change the expected before image so that the transaction will only work if
        //  writers were not quiesced from ownership

        dwControlWordBIExpected = dwControlWordBIExpected & 0xFFFF7FFF;

        //  compute the after image of the control word by performing the transform that
        //  will take us either from state 2 to state 0 or state 2 to state 2

        dwControlWordAI = dwControlWordBIExpected + 0xFFFFFFFF;
        dwControlWordAI = dwControlWordAI - ( ( ( dwControlWordAI + 0xFFFFFFFF ) << 16 ) & 0x80000000 );

        //  attempt to perform the transacted state transition on the control word

        dwControlWordBI = InterlockedCompareExchange( &prwl->dwControlWord, dwControlWordAI, dwControlWordBIExpected );

        //  the transaction failed

        if ( dwControlWordBI != dwControlWordBIExpected ) {
            
            //  the transaction failed because writers were quiesced from ownership

            if ( dwControlWordBI & 0x00008000 ) {
                
                //  leave the lock as a quiesced owner

                SyncpRWLUpdateQuiescedOwnerCountAsWriter( prwl, 0xFFFFFFFF );

                //  we're done

                break;

            //  the transaction failed because another context changed the control word

            } else {
            
                //  try again

                continue;
            }

        //  the transaction succeeded

        } else {
        
            //  there were other writers waiting for ownership of the lock

            if ( dwControlWordAI & 0x00007FFF ) {
                
                //  release the next writer into ownership of the lock

                ReleaseSemaphore( prwl->hsemWriter, 1, NULL );
            }
            
            // we're done

            break;
        }
    }
}

static VOID SyncEnterRWLockAsReader(
    IN OUT  PSYNC_RW_LOCK       prwl
    )
{
    DWORD dwControlWordBIExpected;
    DWORD dwControlWordAI;
    DWORD dwControlWordBI;
    
    //  try forever until we successfully change the lock state

    SYNC_FOREVER {
        
        //  read the current state of the control word as our expected before image

        dwControlWordBIExpected = prwl->dwControlWord;

        //  compute the after image of the control word by performing the global
        //  transform for the EnterAsReader state transition

        dwControlWordAI =   ( dwControlWordBIExpected & 0xFFFF7FFF ) +
                            (   ( dwControlWordBIExpected & 0x80008000 ) == 0x80000000 ?
                                    0x00017FFF :
                                    0x00018000 );

        //  attempt to perform the transacted state transition on the control word

        dwControlWordBI = InterlockedCompareExchange( &prwl->dwControlWord, dwControlWordAI, dwControlWordBIExpected );

        //  the transaction failed or readers were quiesced from ownership

        if ( ( dwControlWordBI ^ dwControlWordBIExpected ) | ( dwControlWordBI & 0x80000000 ) ) {
            
            //  the transaction failed because another context changed the control word

            if ( dwControlWordBI != dwControlWordBIExpected ) {
                
                //  try again

                continue;

            //  the transaction succeeded but readers were quiesced from ownership

            } else {

                //  wait to own the lock as a reader

                SyncpRWLEnterAsReader( prwl, dwControlWordBI );

                //  we now own the lock, so we're done

                break;
            }

        //  the transaction succeeded and readers were not quiesced from ownership

        } else {
        
            //  we now own the lock, so we're done

            break;
        }
    }
}

static BOOL SyncTryEnterRWLockAsReader(
    IN OUT  PSYNC_RW_LOCK       prwl
    )
{
    DWORD dwControlWordBIExpected;
    DWORD dwControlWordAI;
    DWORD dwControlWordBI;
    
    //  try forever until we successfully change the lock state

    SYNC_FOREVER {
        
        //  read the current state of the control word as our expected before image

        dwControlWordBIExpected = prwl->dwControlWord;

        //  change the expected before image so that the transaction will only work if
        //  readers were not quiesced from ownership

        dwControlWordBIExpected = dwControlWordBIExpected & 0x7FFFFFFF;

        //  compute the after image of the control word by performing the global
        //  transform for the EnterAsReader state transition

        dwControlWordAI =   ( dwControlWordBIExpected & 0xFFFF7FFF ) +
                            (   ( dwControlWordBIExpected & 0x80008000 ) == 0x80000000 ?
                                    0x00017FFF :
                                    0x00018000 );

        //  attempt to perform the transacted state transition on the control word

        dwControlWordBI = InterlockedCompareExchange( &prwl->dwControlWord, dwControlWordAI, dwControlWordBIExpected );

        //  the transaction failed

        if ( dwControlWordBI != dwControlWordBIExpected ) {
            
            //  the transaction failed because readers were quiesced from ownership

            if ( dwControlWordBI & 0x80000000 ) {
                
                //  return failure

                return FALSE;

            //  the transaction failed because another context changed the control word

            } else {
            
                //  try again

                continue;
            }

        //  the transaction succeeded

        } else {
        
            // return success

            return TRUE;
        }
    }
}

static VOID SyncLeaveRWLockAsReader(
    IN OUT  PSYNC_RW_LOCK       prwl
    )
{
    DWORD dwControlWordBIExpected;
    DWORD dwControlWordAI;
    DWORD dwControlWordBI;
    
    //  try forever until we successfully change the lock state

    SYNC_FOREVER {
        
        //  read the current state of the control word as our expected before image

        dwControlWordBIExpected = prwl->dwControlWord;

        //  change the expected before image so that the transaction will only work if
        //  readers were not quiesced from ownership

        dwControlWordBIExpected = dwControlWordBIExpected & 0x7FFFFFFF;

        //  compute the after image of the control word by performing the transform that
        //  will take us either from state 1 to state 0 or state 1 to state 1

        dwControlWordAI =   (DWORD)( dwControlWordBIExpected + 0xFFFF0000 +
                            ( ( (LONG_PTR)( (long)( dwControlWordBIExpected + 0xFFFE0000 ) ) >> 31 ) & 0xFFFF8000 ) );

        //  attempt to perform the transacted state transition on the control word

        dwControlWordBI = InterlockedCompareExchange( &prwl->dwControlWord, dwControlWordAI, dwControlWordBIExpected );

        //  the transaction failed

        if ( dwControlWordBI != dwControlWordBIExpected ) {
            
            //  the transaction failed because readers were quiesced from ownership

            if ( dwControlWordBI & 0x80000000 ) {
                
                //  leave the lock as a quiesced owner

                SyncpRWLUpdateQuiescedOwnerCountAsReader( prwl, 0xFFFFFFFF );

                //  we're done

                break;

            //  the transaction failed because another context changed the control word

            } else {
            
                //  try again

                continue;
            }

        //  the transaction succeeded

        } else {
        
            // we're done

            break;
        }
    }
}


mozilla::detail::RWLockImpl::RWLockImpl() {
  static_assert(sizeof(SRWLOCK) <= sizeof(mRWLock), "SRWLOCK is too big!");
  SyncCreateRWLock(NativeHandle(mRWLock));
}

mozilla::detail::RWLockImpl::~RWLockImpl() {SyncDestroyRWLock(NativeHandle(mRWLock));}

bool mozilla::detail::RWLockImpl::tryReadLock() {
  return SyncTryEnterRWLockAsReader(NativeHandle(mRWLock));
}

void mozilla::detail::RWLockImpl::readLock() {
  SyncEnterRWLockAsReader(NativeHandle(mRWLock));
}

void mozilla::detail::RWLockImpl::readUnlock() {
  SyncLeaveRWLockAsReader(NativeHandle(mRWLock));
}

bool mozilla::detail::RWLockImpl::tryWriteLock() {
  return SyncTryEnterRWLockAsWriter(NativeHandle(mRWLock));
}

void mozilla::detail::RWLockImpl::writeLock() {
  SyncEnterRWLockAsWriter(NativeHandle(mRWLock));
}

void mozilla::detail::RWLockImpl::writeUnlock() {
  SyncLeaveRWLockAsWriter(NativeHandle(mRWLock));
}

#undef NativeHandle
