
type value = real
type cost = real
                 
datatype advance = START | ADVANCE_A | ADVANCE_B | ADVANCE_BOTH
type step = advance * cost

fun choose costs =
    case costs of
        (NONE,   NONE,   _) => (START, 0.0)
      | (SOME a, NONE,   _) => (ADVANCE_A, a)
      | (NONE,   SOME b, _) => (ADVANCE_B, b)
      | (SOME _, SOME _, NONE) => raise Fail "Internal error"
      | (SOME a, SOME b, SOME both) =>
        if a < b then
            if both <= a then (ADVANCE_BOTH, both) else (ADVANCE_A, a)
        else
            if both <= b then (ADVANCE_BOTH, both) else (ADVANCE_B, b)

fun cost (p1, p2) =
    Real.abs(p1 - p2)
       
fun costSeries (s1 : value vector) (s2 : value vector) : step vector vector =
    let open Vector

        fun costSeries' (rowAcc : step vector list) j =
            if j = length s1
            then fromList (rev rowAcc)
            else costSeries' (costRow' rowAcc j [] 0 :: rowAcc) (j+1)

        and costRow' (rowAcc : step vector list) j (colAcc : step list) i =
            if i = length s2
            then fromList (rev colAcc)
            else let val c = cost (sub (s1, j), sub (s2, i))
                     val options =
                         (if null rowAcc
                          then NONE
                          else SOME (c + #2 (sub (hd rowAcc, i))),
                          if i = 0
                          then NONE
                          else SOME (c + #2 (hd colAcc)),
                          if null rowAcc orelse i = 0
                          then NONE
                          else SOME (c + #2 (sub (hd rowAcc, i-1))))
                 in
                     costRow' rowAcc j (choose options :: colAcc) (i+1)
                 end
    in
        costSeries' [] 0
    end

fun alignSeries s1 s2 =
    let val cumulativeCosts = costSeries s1 s2
        fun trace (j, i) acc =
            case Vector.sub (Vector.sub (cumulativeCosts, j), i) of
                (START, _) => (i :: acc)
              | (ADVANCE_A, _) => trace (j-1, i) (i :: acc)
              | (ADVANCE_B, _) => trace (j, i-1) acc
              | (ADVANCE_BOTH, _) => trace (j-1, i-1) (i :: acc)

        val sj = Vector.length s1
        val si = Vector.length s2
    in
        Vector.fromList
            (if si = 0 orelse sj = 0
             then []
             else trace (sj-1, si-1) [])
    end

fun preprocess (times : real list, frequencies : real list) :
    real vector * value vector =
    let val pitches =
            map (fn f =>
                    if f < 0.0
                    then 0.0
                    else 12.0 * (Math.log10(f / 220.0) /
                                 Math.log10(2.0)) + 57.0)
                frequencies
    in
        (Vector.fromList times, Vector.fromList pitches)
    end
    
fun read csvFile =
    let fun toNumberPair line =
            case String.fields (fn c => c = #",") line of
                a::b::_ => (case (Real.fromString a, Real.fromString b) of
                                (SOME r1, SOME r2) => (r1, r2)
                              | _ => raise Fail ("Failed to parse numbers: " ^
                                                 line))
              | _ => raise Fail ("Not enough columns: " ^ line)
        fun read' s acc =
            case TextIO.inputLine s of
                SOME line =>
                let val pair = toNumberPair
                                   (String.substring
                                        (line, 0, String.size line - 1))
                in
                    read' s (pair :: acc)
                end
              | NONE => rev acc
        val stream = TextIO.openIn csvFile
        val (timeList, freqList) = ListPair.unzip (read' stream [])
        val _ = TextIO.closeIn stream
    in
        preprocess (timeList, freqList)
    end

fun alignFiles csv1 csv2 =
    let val (times1, pitches1) = read csv1
        val (times2, pitches2) = read csv2
        (* raw alignment returns the index into pitches2 for each
           element in pitches1 *)
        val raw = alignSeries pitches1 pitches2
    in
        List.tabulate (Vector.length raw,
                       fn i => (Vector.sub (times1, i),
                                Vector.sub (times2, Vector.sub (raw, i))))
    end

fun printAlignment alignment =
    app (fn (from, to) =>
            print (Real.toString from ^ "," ^ Real.toString to ^ "\n"))
        alignment
        
fun usage () =
    print ("Usage: pitch-track-align pitch1.csv pitch2.csv\n")

fun main () =
    (case CommandLine.arguments () of
         [csv1, csv2] => printAlignment (alignFiles csv1 csv2)
       | _ => usage ())
    handle exn => 
           (print ("Error: " ^ (exnMessage exn) ^ "\n");
            OS.Process.exit OS.Process.failure)
