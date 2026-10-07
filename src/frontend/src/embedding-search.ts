import { type EmbeddingOptions } from "./api";
import { type EmbeddingProvider, type EmbeddingVariant } from "./types";

export const LOCAL_EMBEDDING_MODEL = "sentence-transformers/all-MiniLM-L6-v2";

export interface EmbeddingSearchSettings {
  variant: EmbeddingVariant;
  provider: EmbeddingProvider;
  model: string;
  attribute: string;
  eventId: string;
  dimensions: string;
  maxItems: string;
  topK: string;
}

export const DEFAULT_EMBEDDING_SEARCH: EmbeddingSearchSettings = {
  variant: "cases_transformers", provider: "hashing", model: "hashing-v1",
  attribute: "concept:name", eventId: "", dimensions: "64", maxItems: "10000", topK: "10",
};

function boundedInteger(value: string, label: string, minimum: number, maximum: number): number {
  const number = value.trim() === "" ? NaN : Number(value);
  if (!Number.isInteger(number) || number < minimum || number > maximum)
    throw new Error(`${label} must be a whole number from ${minimum} to ${maximum}.`);
  return number;
}

/** Keep vector generation separate from ranking: top-k requires a target. */
export function embeddingSearchOptions(settings: EmbeddingSearchSettings, target?: string): EmbeddingOptions {
  if (!settings.model.trim()) throw new Error("Enter an encoder model name.");
  if (!settings.attribute.trim()) throw new Error("Enter a text attribute.");
  const options: EmbeddingOptions = {
    embedding_variant: settings.variant,
    embedding_provider: settings.provider,
    embedding_model: settings.model.trim(),
    embedding_attribute_key: settings.attribute.trim(),
    embedding_dimensions: boundedInteger(settings.dimensions, "Vector dimensions", settings.provider === "transformer" ? 0 : 1, 4096),
    embedding_max_items: boundedInteger(settings.maxItems, "Item limit", 1, 1000000),
  };
  if (settings.variant === "events_transformers" && settings.eventId.trim())
    options.embedding_event_id_key = settings.eventId.trim();
  if (target !== undefined) {
    if (!target.trim()) throw new Error("Enter text to find matching cases or events.");
    options.embedding_target = target;
    options.embedding_top_k = boundedInteger(settings.topK, "Match count", 0, 10000);
  }
  return options;
}

export function embeddingSearchError(cause: unknown): string {
  const message = cause instanceof Error ? cause.message : String(cause);
  if (message.includes("embedding runtime: HTTP status 400"))
    return `${message}. Check that the model name matches the loaded encoder and that the vector dimensions do not exceed its width.`;
  if (message.includes("transformer provider") && message.includes("callback"))
    return `${message}. Connect the Minera server to a running local encoder, then retry.`;
  return message;
}
