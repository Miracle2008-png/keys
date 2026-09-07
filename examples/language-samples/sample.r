# Real R: data frames, apply family, formulas, pipes.
library(stats)

set.seed(42)

simulate <- function(n = 100, mean = 0, sd = 1) {
  values <- rnorm(n, mean = mean, sd = sd)
  data.frame(
    id = seq_len(n),
    value = values,
    group = factor(sample(c("a", "b", "c"), n, replace = TRUE))
  )
}

summarise_groups <- function(df) {
  stopifnot(is.data.frame(df), nrow(df) > 0)

  result <- aggregate(value ~ group, data = df, FUN = function(x) {
    c(mean = mean(x), sd = sd(x), n = length(x))
  })

  result$flagged <- ifelse(result$value[, "mean"] > 0, TRUE, FALSE)
  result
}

samples <- simulate(n = 250, mean = 0.4)
summary_table <- summarise_groups(samples)

model <- lm(value ~ group, data = samples)
print(summary(model))

for (i in seq_len(nrow(summary_table))) {
  if (is.na(summary_table$flagged[i])) next
  cat(sprintf("%s: %.3f\n", summary_table$group[i], summary_table$value[i, "mean"]))
}
