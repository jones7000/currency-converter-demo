import { Injectable } from '@angular/core';
import { createClient, type Client } from '@connectrpc/connect';
import { createGrpcWebTransport } from '@connectrpc/connect-web';

import { CurrencyService } from '../generated/currency_pb';
import type {
  ConstraintsResponse,
  ConvertResponse,
  CurrencyInfo,
  HistoryResponse,
} from '../generated/currency_pb';
import { resolveBackendUrl } from './env';

/**
 * The only place in this frontend that talks to the backend: every
 * component calls through here, never through the generated stub directly,
 * so a transport change is a one-file change. Talks plain gRPC-Web (via
 * Envoy) -- business logic and validation stay entirely on the server;
 * this service is a thin, typed pass-through.
 */
@Injectable({ providedIn: 'root' })
export class CurrencyApiService {
  private readonly client: Client<typeof CurrencyService> = createClient(
    CurrencyService,
    createGrpcWebTransport({ baseUrl: resolveBackendUrl() }),
  );

  listCurrencies(): Promise<{ currencies: CurrencyInfo[] }> {
    return this.client.listCurrencies({});
  }

  convert(sourceCurrency: string, targetCurrency: string, amount: number): Promise<ConvertResponse> {
    return this.client.convert({ sourceCurrency, targetCurrency, amount });
  }

  getHistory(
    sourceCurrency: string,
    targetCurrency: string,
    startDate: string,
    endDate: string,
  ): Promise<HistoryResponse> {
    return this.client.getHistory({ sourceCurrency, targetCurrency, startDate, endDate });
  }

  getConstraints(): Promise<ConstraintsResponse> {
    return this.client.getConstraints({});
  }
}
