
datatype pitch_direction =
         PITCH_NONE |
         PITCH_UP of real |
         PITCH_DOWN of real

type value = pitch_direction
type cost = real

fun choose costs =
    case costs of
        (NONE,   NONE,   _) => 0.0
      | (SOME a, NONE,   _) => a
      | (NONE,   SOME b, _) => b
      | (SOME _, SOME _, NONE) => raise Fail "Internal error"
      | (SOME a, SOME b, SOME both) =>
        if a < b then
            if both <= a then both else a
        else
            if both <= b then both else b

fun cost (p1, p2) =
    let fun together a b = let val diff = Real.abs (a - b) in 
                               if diff < 1.0 then ~1.0
                               else if diff > 3.0 then 1.0
                               else 0.0 
                           end
        fun opposing a b = let val diff = a + b in
                               if diff < 2.0 then 1.0
                               else 2.0
                           end
    in
        case (p1, p2) of
            (PITCH_NONE, PITCH_NONE) => 0.0
          | (PITCH_UP a, PITCH_UP b) => together a b
          | (PITCH_UP a, PITCH_DOWN b) => opposing a b
          | (PITCH_DOWN a, PITCH_UP b) => opposing a b
          | (PITCH_DOWN a, PITCH_DOWN b) => together a b
          | _ => 1.0
    end
       
fun costSeries (s1 : value vector) (s2 : value vector) : cost vector vector =
    let open Vector

        fun costSeries' (rowAcc : cost vector list) j =
            if j = length s1
            then fromList (rev rowAcc)
            else costSeries' (costRow' rowAcc j [] 0 :: rowAcc) (j+1)

        and costRow' (rowAcc : cost vector list) j (colAcc : cost list) i =
            if i = length s2
            then fromList (rev colAcc)
            else let val c = cost (sub (s1, j), sub (s2, i))
                     val options =
                         (if null rowAcc
                          then NONE
                          else SOME (c + sub (hd rowAcc, i)),
                          if i = 0
                          then NONE
                          else SOME (c + hd colAcc),
                          if null rowAcc orelse i = 0
                          then NONE
                          else SOME (c + sub (hd rowAcc, i-1)))
                 in
                     costRow' rowAcc j (choose options :: colAcc) (i+1)
                 end
    in
        costSeries' [] 0
    end

fun alignSeries s1 s2 =
    let val cumulativeCosts = costSeries s1 s2
        fun cost (j, i) = Vector.sub (Vector.sub (cumulativeCosts, j), i)
        fun trace (j, i) acc =
            if i = 0
            then if j = 0
                 then i :: acc
                 else trace (j-1, i) (i :: acc)
            else if j = 0
            then trace (j, i-1) acc
            else let val (a, b, both) =
                         (cost (j-1, i), cost (j, i-1), cost (j-1, i-1))
                 in
                     if a < b then
                         if both <= a
                         then trace (j-1, i-1) (i :: acc)
                         else trace (j-1, i) (i :: acc)
                     else
                         if both <= b
                         then trace (j-1, i-1) (i :: acc)
                         else trace (j, i-1) acc
                 end

        val sj = Vector.length s1
        val si = Vector.length s2
    in
        Vector.fromList
            (if si = 0 orelse sj = 0
             then []
             else trace (sj-1, si-1) [])
    end

fun preprocess (times : real list, frequencies : real list) :
    real vector * value vector * real vector =
    let val pitches =
            map (fn f =>
                    if f < 0.0
                    then 0.0
                    else Real.realRound (12.0 * (Math.log10(f / 220.0) /
                                                 Math.log10(2.0)) + 57.0))
                frequencies
        val values =
            let val acc =
                    foldl (fn (p, (acc, prev)) =>
                              if p <= 0.0 then (PITCH_NONE :: acc, prev)
                              else if prev <= 0.0
                              then (PITCH_UP 0.0 :: acc, p)
                              else if p >= prev
                              then (PITCH_UP (p - prev) :: acc, p)
                              else (PITCH_DOWN (prev - p) :: acc, p))
                          ([], 0.0)
                          pitches
            in
                rev (#1 acc)
            end
(*        val _ =
            app (fn (text, p) =>
                    TextIO.output (TextIO.stdErr, ("[" ^ text ^ "] -> " ^
                                                   Real.toString p ^ "\n")))
                (ListPair.map (fn (PITCH_NONE, p) => (" ", p)
                                | (PITCH_UP d, p) => ("+", p)
                                | (PITCH_DOWN d, p) => ("-", p))
                              (values, pitches))
        val _ = TextIO.output (TextIO.stdErr, "(end)\n");
 *)
        val _ =
            app (fn v =>
                    TextIO.output (TextIO.stdErr,
                                   (case v of
                                        PITCH_NONE => "=0"
                                      | PITCH_UP d => "+" ^ Real.toString d
                                      | PITCH_DOWN d => "-" ^ Real.toString d)
                                   ^ " "))
                values
        val _ = TextIO.output (TextIO.stdErr, " (end)\n");
    in
        (Vector.fromList times,
         Vector.fromList values,
         Vector.fromList pitches)
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

fun meanDiff pitches1 pitches2 mapping =
    let open Vector
        val n = length mapping
        val sumDiff =
            foldli (fn (i, j, acc) => acc +
                                      sub (pitches1, i) -
                                      sub (pitches2, j))
                   0.0 mapping
    in
        if n = 0 then 0.0
        else sumDiff / Real.fromInt n
    end
        
fun alignFiles csv1 csv2 =
    let val (times1, values1, pitches1) = read csv1
        val (times2, values2, pitches2) = read csv2
        (* raw alignment returns the index into pitches2 for each
           element in pitches1 *)
        val raw = alignSeries values1 values2
        val _ = TextIO.output (TextIO.stdErr, "DTW output:\n")
        val _ = Vector.app
                    (fn i => TextIO.output (TextIO.stdErr, Int.toString i ^ " "))
                    raw
        val _ = TextIO.output (TextIO.stdErr, "\n")
        val _ = TextIO.output (TextIO.stdErr,
                               "Mean pitch difference: reference " ^
                               Real.toString (meanDiff pitches1 pitches2 raw)
                               ^ " semitones higher than other track\n")
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
    TextIO.output (TextIO.stdErr,
                   "Usage: pitch-track-align pitch1.csv pitch2.csv\n")

fun main () =
    (case CommandLine.arguments () of
         [csv1, csv2] => printAlignment (alignFiles csv1 csv2)
       | _ => usage ())
    handle exn => 
           (TextIO.output (TextIO.stdErr, "Error: " ^ (exnMessage exn) ^ "\n");
            OS.Process.exit OS.Process.failure)
