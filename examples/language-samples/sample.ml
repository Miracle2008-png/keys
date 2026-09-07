(* Real OCaml: modules, variants, pattern matching, records. *)

type status = Draft | Sent | Paid

type invoice = {
  id : int;
  customer : string;
  amount : float;
  status : status;
}

module Invoice = struct
  let describe inv =
    match inv.amount with
    | a when a > 10000.0 -> "large"
    | a when a > 0.0 -> "standard"
    | _ -> "empty"

  let total invoices =
    List.fold_left (fun acc inv -> acc +. inv.amount) 0.0 invoices

  let overdue invoices =
    invoices
    |> List.filter (fun inv -> inv.status = Sent)
    |> List.sort (fun a b -> compare b.amount a.amount)

  let rec find id = function
    | [] -> None
    | inv :: rest -> if inv.id = id then Some inv else find id rest
end

let () =
  let sample = { id = 1; customer = "Ada"; amount = 42.0; status = Sent } in
  Printf.printf "%s\n" (Invoice.describe sample)
