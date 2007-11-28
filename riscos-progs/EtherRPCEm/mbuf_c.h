// > h.mbuf_c
// equates for mbuf usage
//  This program is free software; you can redistribute it and/or modify it 
//  under the terms of version 2 of the GNU General Public License as 
//  published by the Free Software Foundation;
//
//  This program is distributed in the hope that it will be useful, but WITHOUT 
//  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
//  more details.
//
//  You should have received a copy of the GNU General Public License along with
//  this program; if not, write to the Free Software Foundation, Inc., 59 
//  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//  
//  The full GNU General Public License is included in this distribution in the
//  file called LICENSE.
//
//  The code in this file is (C) 2003 J Ballance and others as far as
//  the above GPL permits

#ifndef __mbuf_c
#define __mbuf_c
// C definition..
#define MSIZE           128
#define MMINOFF         12
#define MTAIL           4
#define MLEN            (MSIZE-MMINOFF-MTAIL) 

struct mbuf {
        struct     mbuf *m_next;// next buffer in chain 
        struct     mbuf *m_list;// next chain (clients only)
        ptrdiff_t  m_off;       // current offset of data from mbuf 
        size_t     m_len;       // amount of data 
  const ptrdiff_t  m_inioff;    // original offset of data from mbuf
  const size_t     m_inilen;    // original byte count (for underlying data) 
  unsigned char    m_type;      // mbuf type (client use only)
  const unsigned char sys[3];   // mbuf manager use only 
};

#define m_dat    m_un.mun_dat
#define m_datp   m_un.mun_datp
/* mbuf types */
#define MT_FREE         0       /* On free list */
#define MT_DATA         1       /* Data */
#define MT_HEADER       2       /* Header */
#define MT_LRXDATA      15      /* Large Data */
typedef struct mbctl
{ /* reserved for mbuf manager use in establishing context */
      int                 opaque;         /* mbuf manager use only */

      /* Client initialises before session is established */
      size_t              mbcsize;        /* size of mbctl structure from
                                           * client */
      unsigned int        mbcvers;        /* client version of mbuf manager
                                           * spec */
      unsigned long       flags;          /* */
      size_t              advminubs;      /* Advisory desired minimum
                                           * underlying block size */
      size_t              advmaxubs;      /* Advisory desired maximum
                                           * underlying block size */
      size_t              mincontig;      /* client required min
                                           * ensure_contig value */
      unsigned long       spare1;         /* Must be set to zero on
                                           * initialisation */
      /* Mbuf manager initialises during session establishment */
      size_t              minubs;         /* Minimum underlying block size */
      size_t              maxubs;         /* Maximum underlying block size */
      size_t              maxcontig;      /* Maximum contiguify block size */
      unsigned long       spare2;         /* Reserved for future use */

      /* Allocation routines */
      struct mbuf *                   /* MBC_DEFAULT */
          (* alloc)
              (struct mbctl *, size_t bytes, void *ptr);

      struct mbuf *                   /* Parameter driven */
          (* alloc_g)
            (struct mbctl *, size_t bytes, void *ptr, unsigned long flags);

      struct mbuf *                   /* MBC_UNSAFE */
          (* alloc_u)
              (struct mbctl *, size_t bytes, void *ptr);
 
      struct mbuf *                   /* MBC_SINGLE */
          (* alloc_s)
              (struct mbctl *, size_t bytes, void *ptr);
 
      struct mbuf *                   /* MBC_CLEAR */
          (* alloc_c)
              (struct mbctl *, size_t bytes, void *ptr);
 
      /* Ensuring routines */
      struct mbuf *
          (* ensure_safe)
              (struct mbctl *, struct mbuf *mp);
 
      struct mbuf *
          (* ensure_contig)
              (struct mbctl *, struct mbuf *mp, size_t bytes);
 
 
 
      /* Freeing routines */
      void
          (* free)
              (struct mbctl *, struct mbuf *mp);
 
      void
          (* freem)
              (struct mbctl *, struct mbuf *mp);
 
      void
          (* dtom_free)
              (struct mbctl *, struct mbuf *mp);
 
      void
          (* dtom_freem)
              (struct mbctl *, struct mbuf *mp);
 
      /* Support routines */
      struct mbuf *                   /* No ownership transfer though */
          (* dtom)
              (struct mbctl *, void *ptr);
 
      int                             /* Client retains mp ownership */
          (* any_unsafe)
              (struct mbctl *, struct mbuf *mp);
 
      int                             /* Client retains mp ownership */
          (* this_unsafe)
              (struct mbctl *, struct mbuf *mp);
 
      size_t                          /* Client retains mp ownership */
          (* count_bytes)
              (struct mbctl *, struct mbuf *mp);
 
      struct mbuf *                   /* Client retains old, new ownership */
          (* cat)
              (struct mbctl *, struct mbuf *old, struct mbuf *newm);
 
      struct mbuf *                   /* Client retains mp ownership */
          (* trim)
              (struct mbctl *, struct mbuf *mp, int bytes, void *ptr);
 
      struct mbuf *                   /* Client retains mp ownership */
          (* copy)
              (struct mbctl *, struct mbuf *mp, size_t off, size_t len);
 
      struct mbuf *                   /* Client retains mp ownership */
          (* copy_p)
              (struct mbctl *, struct mbuf *mp, size_t off, size_t len);
 
      struct mbuf *                   /* Client retains mp ownership */
          (* copy_u)
              (struct mbctl *, struct mbuf *mp, size_t off, size_t len);
 
      struct mbuf *                   /* Client retains mp ownership */
          (* import)
              (struct mbctl *, struct mbuf *mp, size_t bytes, void *ptr);
 
      struct mbuf *                   /* Client retains mp ownership */
          (* export)
              (struct mbctl *, struct mbuf *mp, size_t bytes, void *ptr);
} dci4_mbctl;

#endif
