directory ../../nucleus/traps
directory ../../nucleus/interrupts
directory ../part1
dir ../../util
define gcrpload
set *(int *)0x418 = *(int *)0x40c
end
define pfi
set $i=0
printf "i: %d  term: %d  page:%d track: %d  sector: %d\n", $i, frameinfo[$i].terminal, frameinfo[$i].page, frameinfo[$i].track, frameinfo[$i].sector
set $i+=1
printf "i: %d  term: %d  page:%d track: %d  sector: %d\n", $i, frameinfo[$i].terminal, frameinfo[$i].page, frameinfo[$i].track, frameinfo[$i].sector
set $i+=1
printf "i: %d  term: %d  page:%d track: %d  sector: %d\n", $i, frameinfo[$i].terminal, frameinfo[$i].page, frameinfo[$i].track, frameinfo[$i].sector
set $i+=1
printf "i: %d  term: %d  page:%d track: %d  sector: %d\n", $i, frameinfo[$i].terminal, frameinfo[$i].page, frameinfo[$i].track, frameinfo[$i].sector
set $i+=1
printf "i: %d  term: %d  page:%d track: %d  sector: %d\n", $i, frameinfo[$i].terminal, frameinfo[$i].page, frameinfo[$i].track, frameinfo[$i].sector
set $i+=1
printf "i: %d  term: %d  page:%d track: %d  sector: %d\n", $i, frameinfo[$i].terminal, frameinfo[$i].page, frameinfo[$i].track, frameinfo[$i].sector
set $i+=1
printf "i: %d  term: %d  page:%d track: %d  sector: %d\n", $i, frameinfo[$i].terminal, frameinfo[$i].page, frameinfo[$i].track, frameinfo[$i].sector
set $i+=1
printf "i: %d  term: %d  page:%d track: %d  sector: %d\n", $i, frameinfo[$i].terminal, frameinfo[$i].page, frameinfo[$i].track, frameinfo[$i].sector
set $i+=1
printf "i: %d  term: %d  page:%d track: %d  sector: %d\n", $i, frameinfo[$i].terminal, frameinfo[$i].page, frameinfo[$i].track, frameinfo[$i].sector
set $i+=1
printf "i: %d  term: %d  page:%d track: %d  sector: %d\n", $i, frameinfo[$i].terminal, frameinfo[$i].page, frameinfo[$i].track, frameinfo[$i].sector
end
# see boundary
define pbd
printf "nucleus text:\n"
printf "start: %x\n", (int) start
printf "endt0: %x\n", (int) endt0
printf "sl text:\n"
printf "startt1: %x\n", (int) startt1
printf "etext: %x\n", (int) etext
printf "nucleus data:\n"
printf "startd0: %x\n", (int) startd0
printf "endd0: %x\n", (int) endd0
printf "sl data:\n"
printf "startd1: %x\n", (int) startd1
printf "edata: %x\n", (int) edata
printf "nucleus BSS:\n"
printf "startb0: %x\n", (int) startb0
printf "endb0: %x\n", (int) endb0
printf "sl BSS:\n"
printf "startb1: %x\n", (int) startb1
printf "end: %x\n", (int) end
end
# thank you jay
define printq
 set var $tail = $arg0
 set var $current = $arg0
 set var $flag = 1
 set var $iteration = 0
 while $flag
  if $iteration == 0
   printf "tail->"
   if $current.next == $current.next.p_link[$current.index].next
    printf "head->"
   end
  end
  if $iteration == 1
   printf "head->"
  end
  if ($iteration > 1)
   printf "      "
  end
  printf "%p", $current.next
  if $current.next.parent != (proc_t *)-1
   printf ", parent: %p", $current.next.parent
  end
  if $current.next.child != (proc_t *)-1
   printf ", child: %p", $current.next.child
  end
  if $current.next.sibling != (proc_t *)-1
   printf ", sibling: %p", $current.next.sibling
  end
  printf "\n"
  set var $current = $current.next.p_link[$current.index]
  if $current.next == $tail.next
   set var $flag = 0
  end
  set var $iteration = $iteration + 1
 end
end

define printrq
 printq rq_tl
end

define printsem
 set var $current_semd = $arg0
 set var $current_plink = $current_semd.s_next
 set var $tail = $current_plink
 set var $flag1 = ($current_semd != (semd_t *)-1) && ($current_semd != (semd_t *)0x0)
 set var $flag2 = 1
 while $flag1
  printf "semd_t: %p, addr: %p, value: %i\n", $current_semd, $current_semd.s_semAdd, *$current_semd->s_semAdd
  while $flag2
   printf " |_proc_t: %p", $current_plink.next
   if $current_plink.next.parent != (proc_t *)-1
    printf ", parent: %p", $current_plink.next.parent
   end
   if $current_plink.next.child != (proc_t *)-1
    printf ", child: %p", $current_plink.next.child
   end
   if $current_plink.next.sibling != (proc_t *)-1
    printf ", sibling: %p", $current_plink.next.sibling
   end
   printf "\n"
   set var $current_plink = $current_plink.next.p_link[$current_plink.index]
   if $current_plink.next == $tail.next
    set var $flag2 = 0
   end
  end
  if $current_semd.s_next == (semd_t *)-1
   set var $flag1 = 0
  else
   set var $current_semd = $current_semd.s_next
  end
  set var $flag2 = 1
 end
end

define printasl
 printsem semd_h.head
end

define printboth
 printf "Run Queue:\n"
 printq rq
 printf "\n"
 printf "ASL:\n"
 printsem semd_h
end
# ==============================
b LDST.s:44
b LDST.s:54
b panic
b killproc
# paged
b page.c:530
# cron
b support.c:811
# diskd
b slsyscall2.c:294
# tprocess
b support.c:613
b terminate
b getfreeframe
b pagein
b putframe
b intfloppyhandler
b intterminalhandler
b intclockhandler
b writetoterminal
b trapmmhandler
b page.c:612
# virtualv
b support.c:733
# virtualp
b support.c:746
