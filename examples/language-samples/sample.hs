-- Real Haskell: type classes, monads, pattern matching, where clauses.
{-# LANGUAGE ScopedTypeVariables #-}

module Billing.Invoice
  ( Invoice(..)
  , total
  , overdue
  ) where

import           Data.List (sortBy)
import qualified Data.Map.Strict as Map
import           Data.Maybe (mapMaybe)

data Status = Draft | Sent | Paid deriving (Eq, Ord, Show)

data Invoice = Invoice
  { invoiceId :: Int
  , customer  :: String
  , amount    :: Double
  , status    :: Status
  } deriving (Eq, Show)

class Describable a where
  describe :: a -> String

instance Describable Invoice where
  describe inv
    | amount inv > 10000 = "large " ++ label
    | amount inv > 0     = "standard " ++ label
    | otherwise          = "empty " ++ label
    where
      label = show (invoiceId inv)

total :: [Invoice] -> Double
total = sum . map amount

overdue :: Map.Map Int Invoice -> [Invoice]
overdue = filter ((== Sent) . status) . Map.elems
