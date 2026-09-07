# Real Elixir: modules, pattern matching, pipelines, structs, guards.
defmodule Billing.Invoice do
  @moduledoc "Invoices and the things you do to them."

  @default_currency :usd

  defstruct [:id, :customer, amount: 0.0, status: :draft]

  @type t :: %__MODULE__{
          id: integer(),
          customer: String.t() | nil,
          amount: float(),
          status: atom()
        }

  def describe(%__MODULE__{amount: amount}) when amount > 10_000, do: "large"
  def describe(%__MODULE__{amount: amount}) when amount > 0, do: "standard"
  def describe(%__MODULE__{}), do: "empty"

  def overdue(invoices) when is_list(invoices) do
    invoices
    |> Enum.filter(&(&1.status == :sent))
    |> Enum.sort_by(& &1.amount, :desc)
    |> Enum.take(10)
  end

  def fetch(id) do
    case Repo.get(__MODULE__, id) do
      nil -> {:error, :not_found}
      invoice -> {:ok, invoice}
    end
  end
end
