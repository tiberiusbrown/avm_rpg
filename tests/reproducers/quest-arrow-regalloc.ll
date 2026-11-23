; Reduced from the RPG's full-LTO quest arrow renderer.
; Run with the AVM build of llc from the RPG directory.
;   llc -mtriple=avm -O2 tests/reproducers/quest-arrow-regalloc.ll -o /dev/null
; The affected compiler exits with
;   ran out of registers during register allocation in function 'draw_quest_arrow'
;
; Preserve the three-byte tile loaded after a program-memory indirect call.
; The tile's row remains live across a call returning i24 and supplies a
; coordinate to the five-argument projection call, which also returns i24.

target datalayout = "e-m:e-p:16:8-p1:24:8-i8:8-i16:8-i32:8-i64:8-f16:8-f32:8-n8:16-S8-P1-G0-A0"
target triple = "avm-unknown-arduboyfx"

@marker_callback = external addrspace(1) constant ptr addrspace(1)

declare range(i24 395010, 2103299) i24 @closest_house_exit(i24) addrspace(1)
declare i24 @project_arrow(i16, i16, i16, i16, i8 zeroext) addrspace(1) nounwind memory(none)

define i1 @draw_quest_arrow() addrspace(1) noinline nounwind {
entry:
  %objective = alloca [3 x i8], align 1
  %lookup = load ptr addrspace(1), ptr addrspace(1) @marker_callback, align 1
  %found = call addrspace(1) i1 %lookup(i8 zeroext 0, ptr nonnull align 1 dereferenceable(3) %objective)
  %tile = load i24, ptr %objective, align 1
  %exit = call addrspace(1) i24 @closest_house_exit(i24 %tile)
  %y_bits = lshr i24 %tile, 16
  %y_byte = trunc nuw i24 %y_bits to i8
  %y = zext i8 %y_byte to i16
  %world_y = shl nuw nsw i16 %y, 4
  %arrow = call addrspace(1) i24 @project_arrow(i16 0, i16 0, i16 0, i16 %world_y, i8 zeroext 2)
  %ok = icmp ne i24 %arrow, 0
  ret i1 %ok
}
