import { Code, ConnectError } from '@connectrpc/connect';

/**
 * Maps a caught error to a short, user-facing message. This is the single
 * place that interprets *why* a call failed; components only ever render
 * the resulting string, never branch on the error themselves -- the
 * dumb-client principle applied to error handling too, not just the happy
 * path.
 */
export function toUserMessage(error: unknown): string {
  if (error instanceof ConnectError) {
    switch (error.code) {
      case Code.InvalidArgument:
      case Code.NotFound:
        // Validation messages from the backend are already user-facing.
        return error.message;
      case Code.Unavailable:
        return 'The service is temporarily unavailable. Please try again in a moment.';
      default:
        return 'Something went wrong. Please try again later.';
    }
  }
  return 'Something went wrong. Please try again later.';
}
